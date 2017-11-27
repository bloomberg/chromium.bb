// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_bridge.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/file_stream_forwarder.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/virtual_file_provider_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace arc {

namespace {

// Returns true if it's OK to allow ARC apps to read the given URL.
bool IsUrlAllowed(const GURL& url) {
  // Currently, only externalfile URLs are allowed.
  return url.SchemeIs(content::kExternalFileScheme);
}

// Returns FileSystemContext.
scoped_refptr<storage::FileSystemContext> GetFileSystemContext(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::StoragePartition* storage =
      content::BrowserContext::GetStoragePartitionForSite(context, url);
  return storage->GetFileSystemContext();
}

// Converts the given URL to a FileSystemURL.
storage::FileSystemURL GetFileSystemURL(
    scoped_refptr<storage::FileSystemContext> context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return file_manager::util::CreateIsolatedURLFromVirtualPath(
      *context, /* empty origin */ GURL(),
      chromeos::ExternalFileURLToVirtualPath(url));
}

// Retrieves the file size on the IO thread, and runs the callback on the UI
// thread.
void GetFileSizeOnIOThread(scoped_refptr<storage::FileSystemContext> context,
                           const storage::FileSystemURL& url,
                           ArcFileSystemBridge::GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  context->operation_runner()->GetMetadata(
      url,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::Bind(
          [](ArcFileSystemBridge::GetFileSizeCallback callback,
             base::File::Error result, const base::File::Info& file_info) {
            int64_t size = -1;
            if (result == base::File::FILE_OK && !file_info.is_directory &&
                file_info.size >= 0) {
              size = file_info.size;
            }
            content::BrowserThread::PostTask(
                content::BrowserThread::UI, FROM_HERE,
                base::BindOnce(std::move(callback), size));
          },
          base::Passed(&callback)));
}

// Factory of ArcFileSystemBridge.
class ArcFileSystemBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemBridge,
          ArcFileSystemBridgeFactory> {
 public:
  static constexpr const char* kName = "ArcFileSystemBridgeFactory";

  static ArcFileSystemBridgeFactory* GetInstance() {
    return base::Singleton<ArcFileSystemBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemBridgeFactory>;
  ArcFileSystemBridgeFactory() = default;
  ~ArcFileSystemBridgeFactory() override = default;
};

}  // namespace

ArcFileSystemBridge::ArcFileSystemBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      bridge_service_(bridge_service),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_service_->file_system()->SetHost(this);
}

ArcFileSystemBridge::~ArcFileSystemBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_service_->file_system()->SetHost(nullptr);
}

// static
BrowserContextKeyedServiceFactory* ArcFileSystemBridge::GetFactory() {
  return ArcFileSystemBridgeFactory::GetInstance();
}

// static
ArcFileSystemBridge* ArcFileSystemBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcFileSystemBridgeFactory::GetForBrowserContext(context);
}

void ArcFileSystemBridge::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void ArcFileSystemBridge::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcFileSystemBridge::GetFileName(const std::string& url,
                                      GetFileNameCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::move(callback).Run(url_decoded.ExtractFileName());
}

void ArcFileSystemBridge::GetFileSize(const std::string& url,
                                      GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(-1);
    return;
  }
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url_decoded);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&GetFileSizeOnIOThread, context,
                     GetFileSystemURL(context, url_decoded),
                     std::move(callback)));
}

void ArcFileSystemBridge::GetFileType(const std::string& url,
                                      GetFileTypeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(base::nullopt);
    return;
  }
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url_decoded);
  storage::FileSystemURL file_system_url =
      GetFileSystemURL(context, url_decoded);
  extensions::app_file_handler_util::GetMimeTypeForLocalPath(
      profile_, file_system_url.path(),
      base::Bind(
          [](GetFileTypeCallback callback, const std::string& mime_type) {
            std::move(callback).Run(mime_type.empty()
                                        ? base::nullopt
                                        : base::make_optional(mime_type));
          },
          base::Passed(&callback)));
}

void ArcFileSystemBridge::OnDocumentChanged(
    int64_t watcher_id,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observer_list_)
    observer.OnDocumentChanged(watcher_id, type);
}

void ArcFileSystemBridge::OpenFileToRead(const std::string& url,
                                         OpenFileToReadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  GetFileSize(
      url, base::BindOnce(&ArcFileSystemBridge::OpenFileToReadAfterGetFileSize,
                          weak_ptr_factory_.GetWeakPtr(), url_decoded,
                          std::move(callback)));
}

void ArcFileSystemBridge::OpenFileToReadAfterGetFileSize(
    const GURL& url_decoded,
    OpenFileToReadCallback callback,
    int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (size < 0) {
    LOG(ERROR) << "Failed to get file size " << url_decoded;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  chromeos::DBusThreadManager::Get()->GetVirtualFileProviderClient()->OpenFile(
      size, base::BindOnce(&ArcFileSystemBridge::OnOpenFile,
                           weak_ptr_factory_.GetWeakPtr(), url_decoded,
                           std::move(callback)));
}

void ArcFileSystemBridge::OnOpenFile(const GURL& url_decoded,
                                     OpenFileToReadCallback callback,
                                     const std::string& id,
                                     base::ScopedFD fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Invalid FD";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  DCHECK_EQ(id_to_url_.count(id), 0u);
  id_to_url_[id] = url_decoded;

  mojo::edk::ScopedPlatformHandle platform_handle{
      mojo::edk::PlatformHandle(fd.release())};
  MojoHandle wrapped_handle = MOJO_HANDLE_INVALID;
  MojoResult result = mojo::edk::CreatePlatformHandleWrapper(
      std::move(platform_handle), &wrapped_handle);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to wrap handle: " << result;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  std::move(callback).Run(mojo::ScopedHandle(mojo::Handle(wrapped_handle)));
}

bool ArcFileSystemBridge::HandleReadRequest(const std::string& id,
                                            int64_t offset,
                                            int64_t size,
                                            base::ScopedFD pipe_write_end) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it_url = id_to_url_.find(id);
  if (it_url == id_to_url_.end()) {
    LOG(ERROR) << "Invalid ID: " << id;
    return false;
  }

  // Add a new element to the list, and get an iterator.
  // NOTE: std::list iterators never get invalidated as long as the pointed
  // element is alive.
  file_stream_forwarders_.emplace_front();
  auto it_forwarder = file_stream_forwarders_.begin();

  const GURL& url = it_url->second;
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url);
  *it_forwarder = FileStreamForwarderPtr(new FileStreamForwarder(
      context, GetFileSystemURL(context, url), offset, size,
      std::move(pipe_write_end),
      base::BindOnce(&ArcFileSystemBridge::OnReadRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), id, it_forwarder)));
  return true;
}

bool ArcFileSystemBridge::HandleIdReleased(const std::string& id) {
  return id_to_url_.erase(id) != 0;
}

void ArcFileSystemBridge::OnReadRequestCompleted(
    const std::string& id,
    std::list<FileStreamForwarderPtr>::iterator it,
    bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG_IF(ERROR, !result) << "Failed to read " << id;
  file_stream_forwarders_.erase(it);
}

}  // namespace arc
