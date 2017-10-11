// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_impl.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "ppapi/shared_impl/ppapi_constants.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/origin.h"

// Currently this uses the PluginPrivateFileSystem as the previous CDMs ran
// as pepper plugins and we need to be able to access any existing files.
// TODO(jrummell): Switch to using a separate file system once CDMs no
// longer run as pepper plugins.

namespace content {

namespace {

// File map shared by all CdmStorageImpl objects to prevent read/write race.
// A lock must be acquired before opening a file to ensure that the file is not
// currently in use. Once the file is opened the file map must be updated to
// include the callback used to close the file. The lock must be held until
// the file is closed.
class FileLockMap {
 public:
  using Key = CdmStorageImpl::FileLockKey;

  FileLockMap() = default;
  ~FileLockMap() = default;

  // Acquire a lock on the file represented by |key|. Returns true if |key|
  // is not currently in use, false otherwise.
  bool AcquireFileLock(const Key& key) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Add a new entry. If |key| already has an entry, emplace() tells so
    // with the second piece of the returned value and does not modify
    // the original.
    return file_lock_map_.emplace(key, base::Closure()).second;
  }

  // Update the entry for the file represented by |key| so that
  // |on_close_callback| will be called when the lock is released.
  void SetOnCloseCallback(const Key& key, base::Closure on_close_callback) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    DCHECK(!file_lock_map_.empty());
    auto entry = file_lock_map_.find(key);
    DCHECK(entry != file_lock_map_.end());
    entry->second = std::move(on_close_callback);
  }

  // Release the lock held on the file represented by |key|. If
  // |on_close_callback| has been set, run it before releasing the lock.
  void ReleaseFileLock(const Key& key) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    auto entry = file_lock_map_.find(key);
    if (entry == file_lock_map_.end()) {
      NOTREACHED() << "Unable to relase lock on file " << key.file_name;
      return;
    }

    if (entry->second)
      entry->second.Run();
    file_lock_map_.erase(entry);
  }

 private:
  // Note that this map is never deleted. As entries are removed when a file
  // is closed, it should never get too large.
  std::map<Key, base::Closure> file_lock_map_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(FileLockMap);
};

FileLockMap* GetFileLockMap() {
  static auto* file_lock_map = new FileLockMap();
  return file_lock_map;
}

// mojom::CdmStorage::Open() returns a mojom::CdmFileReleaser reference to keep
// track of the file being used. This object is created when the file is being
// passed to the client. When the client is done using the file, the connection
// should be broken and this will release the lock held on the file.
class CdmFileReleaserImpl final : public media::mojom::CdmFileReleaser {
 public:
  using Key = CdmStorageImpl::FileLockKey;

  explicit CdmFileReleaserImpl(const Key& key) : key_(key) {
    DVLOG(1) << __func__;
  }

  ~CdmFileReleaserImpl() override {
    DVLOG(1) << __func__;
    GetFileLockMap()->ReleaseFileLock(key_);
  }

 private:
  Key key_;

  DISALLOW_COPY_AND_ASSIGN(CdmFileReleaserImpl);
};

}  // namespace

CdmStorageImpl::FileLockKey::FileLockKey(const std::string& cdm_file_system_id,
                                         const url::Origin& origin,
                                         const std::string& file_name)
    : cdm_file_system_id(cdm_file_system_id),
      origin(origin),
      file_name(file_name) {}

bool CdmStorageImpl::FileLockKey::operator<(const FileLockKey& other) const {
  return std::tie(cdm_file_system_id, origin, file_name) <
         std::tie(other.cdm_file_system_id, other.origin, other.file_name);
}

// static
void CdmStorageImpl::Create(RenderFrameHost* render_frame_host,
                            const std::string& cdm_file_system_id,
                            media::mojom::CdmStorageRequest request) {
  DVLOG(3) << __func__;
  DCHECK(!render_frame_host->GetLastCommittedOrigin().unique())
      << "Invalid origin specified for CdmStorageImpl::Create";

  // Take a reference to the FileSystemContext.
  scoped_refptr<storage::FileSystemContext> file_system_context;
  StoragePartition* storage_partition =
      render_frame_host->GetProcess()->GetStoragePartition();
  if (storage_partition)
    file_system_context = storage_partition->GetFileSystemContext();

  // The created object is bound to (and owned by) |request|.
  new CdmStorageImpl(render_frame_host, cdm_file_system_id,
                     std::move(file_system_context), std::move(request));
}

// static
bool CdmStorageImpl::IsValidCdmFileSystemId(
    const std::string& cdm_file_system_id) {
  // To be compatible with PepperFileSystemBrowserHost::GeneratePluginId(),
  // |cdm_file_system_id| must contain only letters (A-Za-z), digits(0-9),
  // or "._-".
  for (const auto& ch : cdm_file_system_id) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '.' &&
        ch != '_' && ch != '-') {
      return false;
    }
  }

  // Also ensure that |cdm_file_system_id| contains at least 1 character.
  return !cdm_file_system_id.empty();
}

CdmStorageImpl::CdmStorageImpl(
    RenderFrameHost* render_frame_host,
    const std::string& cdm_file_system_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    media::mojom::CdmStorageRequest request)
    : FrameServiceBase(render_frame_host, std::move(request)),
      cdm_file_system_id_(cdm_file_system_id),
      file_system_context_(std::move(file_system_context)),
      child_process_id_(render_frame_host->GetProcess()->GetID()),
      weak_factory_(this) {}

CdmStorageImpl::~CdmStorageImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If any file locks were taken in Open() and never made it to OnFileOpened(),
  // release them now as OnFileOpened() will never run.
  for (const auto& file_lock_key : pending_open_)
    GetFileLockMap()->ReleaseFileLock(file_lock_key);
}

void CdmStorageImpl::Open(const std::string& file_name, OpenCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsValidCdmFileSystemId(cdm_file_system_id_)) {
    DVLOG(1) << "CdmStorageImpl not initialized properly.";
    std::move(callback).Run(Status::kFailure, base::File(), nullptr);
    return;
  }

  if (file_name.empty()) {
    DVLOG(1) << "No file specified.";
    std::move(callback).Run(Status::kFailure, base::File(), nullptr);
    return;
  }

  FileLockKey file_lock_key(cdm_file_system_id_, origin(), file_name);
  if (!GetFileLockMap()->AcquireFileLock(file_lock_key)) {
    DVLOG(1) << "File " << file_name << " is already in use.";
    std::move(callback).Run(Status::kInUse, base::File(), nullptr);
    return;
  }

  std::string fsid =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypePluginPrivate, ppapi::kPluginPrivateRootName,
          base::FilePath());
  if (!storage::ValidateIsolatedFileSystemId(fsid)) {
    DVLOG(1) << "Invalid file system ID.";
    GetFileLockMap()->ReleaseFileLock(file_lock_key);
    std::move(callback).Run(Status::kFailure, base::File(), nullptr);
    return;
  }

  // In case this object gets destroyed before OpenPluginPrivateFileSystem()
  // completes, keep track of |file_lock_key| so that it can be released if
  // OnFileSystemOpened() or OnFileOpened() never get called.
  pending_open_.insert(file_lock_key);

  // Grant full access of isolated file system to child process.
  ChildProcessSecurityPolicy::GetInstance()->GrantCreateReadWriteFileSystem(
      child_process_id_, fsid);

  file_system_context_->OpenPluginPrivateFileSystem(
      origin().GetURL(), storage::kFileSystemTypePluginPrivate, fsid,
      cdm_file_system_id_, storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&CdmStorageImpl::OnFileSystemOpened,
                 weak_factory_.GetWeakPtr(), file_lock_key, fsid,
                 base::Passed(&callback)));
}

void CdmStorageImpl::OnFileSystemOpened(const FileLockKey& file_lock_key,
                                        const std::string& fsid,
                                        OpenCallback callback,
                                        base::File::Error error) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error != base::File::FILE_OK) {
    DVLOG(1) << "Unable to access the file system.";
    pending_open_.erase(file_lock_key);
    GetFileLockMap()->ReleaseFileLock(file_lock_key);
    std::move(callback).Run(Status::kFailure, base::File(), nullptr);
    return;
  }

  std::string root = storage::GetIsolatedFileSystemRootURIString(
                         origin().GetURL(), fsid, ppapi::kPluginPrivateRootName)
                         .append(file_lock_key.file_name);
  storage::FileSystemURL file_url = file_system_context_->CrackURL(GURL(root));
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());
  operation_context->set_allowed_bytes_growth(storage::QuotaManager::kNoLimit);
  int flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
              base::File::FLAG_WRITE;
  DVLOG(1) << "Opening " << file_url.DebugString();
  file_util->CreateOrOpen(
      std::move(operation_context), file_url, flags,
      base::Bind(&CdmStorageImpl::OnFileOpened, weak_factory_.GetWeakPtr(),
                 file_lock_key, base::Passed(&callback)));
}

void CdmStorageImpl::OnFileOpened(const FileLockKey& file_lock_key,
                                  OpenCallback callback,
                                  base::File file,
                                  const base::Closure& on_close_callback) {
  // |on_close_callback| should be called after the |file| is closed in the
  // child process. See AsyncFileUtil for details.
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // |file_lock_key| will either be used by CdmFileReleaserImpl on a successful
  // open or released if |file| is not valid, so the open is no longer pending.
  pending_open_.erase(file_lock_key);

  if (!file.IsValid()) {
    DLOG(WARNING) << "Unable to open file " << file_lock_key.file_name
                  << ", error: "
                  << base::File::ErrorToString(file.error_details());
    GetFileLockMap()->ReleaseFileLock(file_lock_key);
    std::move(callback).Run(Status::kFailure, base::File(), nullptr);
    return;
  }

  GetFileLockMap()->SetOnCloseCallback(file_lock_key,
                                       std::move(on_close_callback));

  // When the connection to |releaser| is closed, ReleaseFileLock() will be
  // called. This will release the lock on the file and cause
  // |on_close_callback| to be run.
  media::mojom::CdmFileReleaserAssociatedPtrInfo releaser;
  mojo::MakeStrongAssociatedBinding(
      std::make_unique<CdmFileReleaserImpl>(file_lock_key),
      mojo::MakeRequest(&releaser));
  std::move(callback).Run(Status::kSuccess, std::move(file),
                          std::move(releaser));
}

}  // namespace content
