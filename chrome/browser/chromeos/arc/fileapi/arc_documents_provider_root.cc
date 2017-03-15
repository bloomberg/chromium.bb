// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;
using EntryList = storage::AsyncFileUtil::EntryList;

namespace arc {

namespace {

// Computes a file name for a document.
// TODO(crbug.com/675868): Consolidate with the similar logic for Drive.
base::FilePath::StringType GetFileNameForDocument(
    const mojom::DocumentPtr& document) {
  base::FilePath::StringType filename = document->display_name;

  // Replace path separators appearing in the file name.
  // Chrome OS is POSIX and kSeparators is "/".
  base::ReplaceChars(filename, base::FilePath::kSeparators, "_", &filename);

  // Do not allow an empty file name and all-dots file names.
  if (filename.empty() ||
      filename.find_first_not_of('.', 0) == std::string::npos) {
    filename = "_";
  }

  // Since Chrome detects MIME type from file name extensions, we need to change
  // the file name extension of the document if it does not match with its MIME
  // type.
  // For example, Audio Media Provider presents a music file with its title as
  // the file name.
  base::FilePath::StringType extension =
      base::ToLowerASCII(base::FilePath(filename).Extension());
  if (!extension.empty())
    extension = extension.substr(1);  // Strip the leading dot.
  std::vector<base::FilePath::StringType> possible_extensions =
      GetExtensionsForArcMimeType(document->mime_type);
  if (!possible_extensions.empty() &&
      std::find(possible_extensions.begin(), possible_extensions.end(),
                extension) == possible_extensions.end()) {
    filename =
        base::FilePath(filename).AddExtension(possible_extensions[0]).value();
  }

  return filename;
}

}  // namespace

// static
const int64_t ArcDocumentsProviderRoot::kInvalidWatcherId = -1;
// static
const uint64_t ArcDocumentsProviderRoot::kInvalidWatcherRequestId = 0;
// static
const ArcDocumentsProviderRoot::WatcherData
    ArcDocumentsProviderRoot::kInvalidWatcherData = {kInvalidWatcherId,
                                                     kInvalidWatcherRequestId};

ArcDocumentsProviderRoot::ArcDocumentsProviderRoot(
    const std::string& authority,
    const std::string& root_document_id)
    : authority_(authority),
      root_document_id_(root_document_id),
      weak_ptr_factory_(this) {}

ArcDocumentsProviderRoot::~ArcDocumentsProviderRoot() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (observer_wrapper_)
    file_system_operation_runner_util::RemoveObserverOnIOThread(
        std::move(observer_wrapper_));
}

void ArcDocumentsProviderRoot::GetFileInfo(
    const base::FilePath& path,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResolveToDocumentId(
      path, base::Bind(&ArcDocumentsProviderRoot::GetFileInfoWithDocumentId,
                       weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::ReadDirectory(
    const base::FilePath& path,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResolveToDocumentId(
      path, base::Bind(&ArcDocumentsProviderRoot::ReadDirectoryWithDocumentId,
                       weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::AddWatcher(
    const base::FilePath& path,
    const WatcherCallback& watcher_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (path_to_watcher_data_.count(path)) {
    callback.Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  uint64_t watcher_request_id = next_watcher_request_id_++;
  path_to_watcher_data_.insert(
      std::make_pair(path, WatcherData{kInvalidWatcherId, watcher_request_id}));
  ResolveToDocumentId(
      path, base::Bind(&ArcDocumentsProviderRoot::AddWatcherWithDocumentId,
                       weak_ptr_factory_.GetWeakPtr(), path, watcher_request_id,
                       watcher_callback));

  // HACK: Invoke |callback| immediately.
  //
  // TODO(crbug.com/698624): Remove this hack. It was introduced because Files
  // app freezes until AddWatcher() finishes, but it should be handled in Files
  // app rather than here.
  callback.Run(base::File::FILE_OK);
}

void ArcDocumentsProviderRoot::RemoveWatcher(const base::FilePath& path,
                                             const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = path_to_watcher_data_.find(path);
  if (iter == path_to_watcher_data_.end()) {
    callback.Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  int64_t watcher_id = iter->second.id;
  path_to_watcher_data_.erase(iter);
  if (watcher_id == kInvalidWatcherId) {
    // This is an invalid watcher. No need to send a request to the remote
    // service.
    callback.Run(base::File::FILE_OK);
    return;
  }
  file_system_operation_runner_util::RemoveWatcherOnIOThread(
      watcher_id,
      base::Bind(&ArcDocumentsProviderRoot::OnWatcherRemoved,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::ResolveToContentUrl(
    const base::FilePath& path,
    const ResolveToContentUrlCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResolveToDocumentId(
      path,
      base::Bind(&ArcDocumentsProviderRoot::ResolveToContentUrlWithDocumentId,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::OnWatchersCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Mark all watchers invalid.
  for (auto& entry : path_to_watcher_data_)
    entry.second = kInvalidWatcherData;
}

void ArcDocumentsProviderRoot::GetFileInfoWithDocumentId(
    const GetFileInfoCallback& callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (document_id.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, base::File::Info());
    return;
  }
  // Specially handle the root directory since Files app does not update the
  // list of file systems (left pane) until all volumes respond to GetMetadata
  // requests to root directories.
  if (document_id == root_document_id_) {
    base::File::Info info;
    info.size = -1;
    info.is_directory = true;
    info.is_symbolic_link = false;
    info.last_modified = info.last_accessed = info.creation_time =
        base::Time::UnixEpoch();  // arbitrary
    callback.Run(base::File::FILE_OK, info);
    return;
  }
  file_system_operation_runner_util::GetDocumentOnIOThread(
      authority_, document_id,
      base::Bind(&ArcDocumentsProviderRoot::GetFileInfoWithDocument,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::GetFileInfoWithDocument(
    const GetFileInfoCallback& callback,
    mojom::DocumentPtr document) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (document.is_null()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, base::File::Info());
    return;
  }
  base::File::Info info;
  info.size = document->size;
  info.is_directory = document->mime_type == kAndroidDirectoryMimeType;
  info.is_symbolic_link = false;
  info.last_modified = info.last_accessed = info.creation_time =
      base::Time::FromJavaTime(document->last_modified);
  callback.Run(base::File::FILE_OK, info);
}

void ArcDocumentsProviderRoot::ReadDirectoryWithDocumentId(
    const ReadDirectoryCallback& callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (document_id.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, EntryList(),
                 false /* has_more */);
    return;
  }
  ReadDirectoryInternal(
      document_id,
      base::Bind(
          &ArcDocumentsProviderRoot::ReadDirectoryWithNameToThinDocumentMap,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::ReadDirectoryWithNameToThinDocumentMap(
    const ReadDirectoryCallback& callback,
    base::File::Error error,
    NameToThinDocumentMap mapping) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (error != base::File::FILE_OK) {
    callback.Run(error, EntryList(), false /* has_more */);
    return;
  }
  EntryList entry_list;
  for (const auto& pair : mapping) {
    entry_list.emplace_back(pair.first, pair.second.is_directory
                                            ? storage::DirectoryEntry::DIRECTORY
                                            : storage::DirectoryEntry::FILE);
  }
  callback.Run(base::File::FILE_OK, entry_list, false /* has_more */);
}

void ArcDocumentsProviderRoot::AddWatcherWithDocumentId(
    const base::FilePath& path,
    uint64_t watcher_request_id,
    const WatcherCallback& watcher_callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsWatcherInflightRequestCanceled(path, watcher_request_id))
    return;

  if (document_id.empty()) {
    DCHECK(path_to_watcher_data_.count(path));
    path_to_watcher_data_[path] = kInvalidWatcherData;
    return;
  }

  // Start observing ArcFileSystemOperationRunner if we have not.
  if (!observer_wrapper_) {
    observer_wrapper_ =
        new file_system_operation_runner_util::ObserverIOThreadWrapper(this);
    file_system_operation_runner_util::AddObserverOnIOThread(observer_wrapper_);
  }

  file_system_operation_runner_util::AddWatcherOnIOThread(
      authority_, document_id, watcher_callback,
      base::Bind(&ArcDocumentsProviderRoot::OnWatcherAdded,
                 weak_ptr_factory_.GetWeakPtr(), path, watcher_request_id));
}

void ArcDocumentsProviderRoot::OnWatcherAdded(const base::FilePath& path,
                                              uint64_t watcher_request_id,
                                              int64_t watcher_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsWatcherInflightRequestCanceled(path, watcher_request_id)) {
    file_system_operation_runner_util::RemoveWatcherOnIOThread(
        watcher_id,
        base::Bind(&ArcDocumentsProviderRoot::OnWatcherAddedButRemoved,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(path_to_watcher_data_.count(path));
  path_to_watcher_data_[path] =
      WatcherData{watcher_id < 0 ? kInvalidWatcherId : watcher_id,
                  kInvalidWatcherRequestId};
}

void ArcDocumentsProviderRoot::OnWatcherAddedButRemoved(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Ignore |success|.
}

void ArcDocumentsProviderRoot::OnWatcherRemoved(const StatusCallback& callback,
                                                bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(success ? base::File::FILE_OK : base::File::FILE_ERROR_FAILED);
}

bool ArcDocumentsProviderRoot::IsWatcherInflightRequestCanceled(
    const base::FilePath& path,
    uint64_t watcher_request_id) const {
  auto iter = path_to_watcher_data_.find(path);
  return (iter == path_to_watcher_data_.end() ||
          iter->second.inflight_request_id != watcher_request_id);
}

void ArcDocumentsProviderRoot::ResolveToContentUrlWithDocumentId(
    const ResolveToContentUrlCallback& callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (document_id.empty()) {
    callback.Run(GURL());
    return;
  }
  callback.Run(BuildDocumentUrl(authority_, document_id));
}

void ArcDocumentsProviderRoot::ResolveToDocumentId(
    const base::FilePath& path,
    const ResolveToDocumentIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  ResolveToDocumentIdRecursively(root_document_id_, components, callback);
}

void ArcDocumentsProviderRoot::ResolveToDocumentIdRecursively(
    const std::string& document_id,
    const std::vector<base::FilePath::StringType>& components,
    const ResolveToDocumentIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (components.empty()) {
    callback.Run(document_id);
    return;
  }
  ReadDirectoryInternal(
      document_id,
      base::Bind(&ArcDocumentsProviderRoot::
                     ResolveToDocumentIdRecursivelyWithNameToThinDocumentMap,
                 weak_ptr_factory_.GetWeakPtr(), components, callback));
}

void ArcDocumentsProviderRoot::
    ResolveToDocumentIdRecursivelyWithNameToThinDocumentMap(
        const std::vector<base::FilePath::StringType>& components,
        const ResolveToDocumentIdCallback& callback,
        base::File::Error error,
        NameToThinDocumentMap mapping) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!components.empty());
  if (error != base::File::FILE_OK) {
    callback.Run(std::string());
    return;
  }
  auto iter = mapping.find(components[0]);
  if (iter == mapping.end()) {
    callback.Run(std::string());
    return;
  }
  ResolveToDocumentIdRecursively(iter->second.document_id,
                                 std::vector<base::FilePath::StringType>(
                                     components.begin() + 1, components.end()),
                                 callback);
}

void ArcDocumentsProviderRoot::ReadDirectoryInternal(
    const std::string& document_id,
    const ReadDirectoryInternalCallback& callback) {
  file_system_operation_runner_util::GetChildDocumentsOnIOThread(
      authority_, document_id,
      base::Bind(
          &ArcDocumentsProviderRoot::ReadDirectoryInternalWithChildDocuments,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void ArcDocumentsProviderRoot::ReadDirectoryInternalWithChildDocuments(
    const ReadDirectoryInternalCallback& callback,
    base::Optional<std::vector<mojom::DocumentPtr>> maybe_children) {
  if (!maybe_children) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, NameToThinDocumentMap());
    return;
  }
  std::vector<mojom::DocumentPtr>& children = maybe_children.value();

  // Sort entries to keep the mapping stable as far as possible.
  std::sort(children.begin(), children.end(),
            [](const mojom::DocumentPtr& a, const mojom::DocumentPtr& b) {
              return a->document_id < b->document_id;
            });

  NameToThinDocumentMap mapping;
  std::map<base::FilePath::StringType, int> suffix_counters;

  for (const mojom::DocumentPtr& document : children) {
    base::FilePath::StringType filename = GetFileNameForDocument(document);

    if (mapping.count(filename) > 0) {
      // Resolve a conflict by adding a suffix.
      int& suffix_counter = suffix_counters[filename];
      while (true) {
        ++suffix_counter;
        std::string suffix = base::StringPrintf(" (%d)", suffix_counter);
        base::FilePath::StringType new_filename =
            base::FilePath(filename).InsertBeforeExtensionASCII(suffix).value();
        if (mapping.count(new_filename) == 0) {
          filename = new_filename;
          break;
        }
      }
    }

    DCHECK_EQ(0u, mapping.count(filename));

    mapping[filename] =
        ThinDocument{document->document_id,
                     document->mime_type == kAndroidDirectoryMimeType};
  }

  callback.Run(base::File::FILE_OK, std::move(mapping));
}

}  // namespace arc
