// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner_util.h"
#include "components/arc/common/file_system.mojom.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/watcher_manager.h"

class GURL;

namespace arc {

// Represents a file system root in Android Documents Provider.
//
// All methods must be called on the IO thread.
// If this object is deleted while there are in-flight operations, callbacks
// for those operations will be never called.
class ArcDocumentsProviderRoot : public ArcFileSystemOperationRunner::Observer {
 public:
  using GetFileInfoCallback = storage::AsyncFileUtil::GetFileInfoCallback;
  using ReadDirectoryCallback = storage::AsyncFileUtil::ReadDirectoryCallback;
  using ChangeType = storage::WatcherManager::ChangeType;
  using WatcherCallback = base::Callback<void(ChangeType type)>;
  using StatusCallback = base::Callback<void(base::File::Error error)>;
  using ResolveToContentUrlCallback =
      base::Callback<void(const GURL& content_url)>;

  ArcDocumentsProviderRoot(const std::string& authority,
                           const std::string& root_document_id);
  ~ArcDocumentsProviderRoot() override;

  // Queries information of a file just like AsyncFileUtil.GetFileInfo().
  void GetFileInfo(const base::FilePath& path,
                   const GetFileInfoCallback& callback);

  // Queries a list of files under a directory just like
  // AsyncFileUtil.ReadDirectory().
  void ReadDirectory(const base::FilePath& path,
                     const ReadDirectoryCallback& callback);

  // Installs a document watcher.
  //
  // It is not allowed to install multiple watchers at the same file path;
  // if attempted, duplicated requests will fail.
  //
  // Currently, watchers can be installed only on directories, and only
  // directory content changes are notified. The result of installing a watcher
  // to a non-directory in unspecified.
  //
  // NOTES ABOUT CORRECTNESS AND CONSISTENCY:
  //
  // Document watchers are not always correct and they may miss some updates or
  // even notify incorrect update events for several reasons, such as:
  //
  //   - Directory moves: Currently a watcher will misbehave if the watched
  //     directory is moved to another location. This is acceptable for now
  //     since we whitelist MediaDocumentsProvider only.
  //   - Duplicated file name handling in this class: The same reason as
  //     directory moves. This may happen even with MediaDocumentsProvider,
  //     but the chance will not be very high.
  //   - File system operation races: For example, an watcher can be installed
  //     to a non-directory in a race condition.
  //   - Broken DocumentsProviders: For example, we get no notification if a
  //     document provider does not call setNotificationUri().
  //
  // However, consistency of installed watchers is guaranteed. That is, after
  // a watcher is installed on a file path X, an attempt to uninstall a watcher
  // at X will always succeed.
  //
  // Unfortunately it is too difficult (or maybe theoretically impossible) to
  // implement a perfect Android document watcher which never misses document
  // updates. So the current implementation gives up correctness, but instead
  // focuses on following two goals:
  //
  //   1. Keep the implementation simple, rather than trying hard to catch
  //      race conditions or minor cases. Even if we return wrong results, the
  //      worst consequence is just that users do not see the latest contents
  //      until they refresh UI.
  //
  //   2. Keep consistency of installed watchers so that the caller can avoid
  //      dangling watchers.
  void AddWatcher(const base::FilePath& path,
                  const WatcherCallback& watcher_callback,
                  const StatusCallback& callback);

  // Uninstalls a document watcher.
  // See the documentation of AddWatcher() above.
  void RemoveWatcher(const base::FilePath& path,
                     const StatusCallback& callback);

  // Resolves a file path into a content:// URL pointing to the file
  // on DocumentsProvider. Returns URL that can be passed to
  // ArcContentFileSystemFileSystemReader to read the content.
  // On errors, an invalid GURL is returned.
  void ResolveToContentUrl(const base::FilePath& path,
                           const ResolveToContentUrlCallback& callback);

  // ArcFileSystemOperationRunner::Observer overrides:
  void OnWatchersCleared() override;

 private:
  // Thin representation of a document in documents provider.
  struct ThinDocument {
    std::string document_id;
    bool is_directory;
  };

  // Represents the status of a document watcher.
  struct WatcherData {
    // ID of a watcher in the remote file system service.
    //
    // Valid IDs are represented by positive integers. An invalid watcher is
    // represented by |kInvalidWatcherId|, which occurs in several cases:
    //
    // - AddWatcher request is still in-flight. In this case, a valid ID is set
    //   to |inflight_request_id|.
    //
    // - The remote file system service notified us that it stopped and all
    //   watchers were forgotten. Such watchers are still tracked here, but they
    //   are not known by the remote service.
    int64_t id;

    // A unique ID of AddWatcher() request.
    //
    // While AddWatcher() is in-flight, a positive integer is set to this
    // variable, and |id| is |kInvalidWatcherId|. Otherwise it is set to
    // |kInvalidWatcherRequestId|.
    uint64_t inflight_request_id;
  };

  static const int64_t kInvalidWatcherId;
  static const uint64_t kInvalidWatcherRequestId;
  static const WatcherData kInvalidWatcherData;

  // Mapping from a file name to a ThinDocument.
  using NameToThinDocumentMap =
      std::map<base::FilePath::StringType, ThinDocument>;

  using ResolveToDocumentIdCallback =
      base::Callback<void(const std::string& document_id)>;
  using ReadDirectoryInternalCallback =
      base::Callback<void(base::File::Error error,
                          NameToThinDocumentMap mapping)>;

  void GetFileInfoWithDocumentId(const GetFileInfoCallback& callback,
                                 const std::string& document_id);
  void GetFileInfoWithDocument(const GetFileInfoCallback& callback,
                               mojom::DocumentPtr document);

  void ReadDirectoryWithDocumentId(const ReadDirectoryCallback& callback,
                                   const std::string& document_id);
  void ReadDirectoryWithNameToThinDocumentMap(
      const ReadDirectoryCallback& callback,
      base::File::Error error,
      NameToThinDocumentMap mapping);

  void AddWatcherWithDocumentId(const base::FilePath& path,
                                uint64_t watcher_request_id,
                                const WatcherCallback& watcher_callback,
                                const std::string& document_id);
  void OnWatcherAdded(const base::FilePath& path,
                      uint64_t watcher_request_id,
                      int64_t watcher_id);
  void OnWatcherAddedButRemoved(bool success);

  void OnWatcherRemoved(const StatusCallback& callback, bool success);

  // Returns true if the specified watcher request has been canceled.
  // This function should be called only while the request is in-flight.
  bool IsWatcherInflightRequestCanceled(const base::FilePath& path,
                                        uint64_t watcher_request_id) const;

  void ResolveToContentUrlWithDocumentId(
      const ResolveToContentUrlCallback& callback,
      const std::string& document_id);

  // Resolves |path| to a document ID. Failures are indicated by an empty
  // document ID.
  void ResolveToDocumentId(const base::FilePath& path,
                           const ResolveToDocumentIdCallback& callback);
  void ResolveToDocumentIdRecursively(
      const std::string& document_id,
      const std::vector<base::FilePath::StringType>& components,
      const ResolveToDocumentIdCallback& callback);
  void ResolveToDocumentIdRecursivelyWithNameToThinDocumentMap(
      const std::vector<base::FilePath::StringType>& components,
      const ResolveToDocumentIdCallback& callback,
      base::File::Error error,
      NameToThinDocumentMap mapping);

  // Enumerates child documents of a directory specified by |document_id|.
  // The result is returned as a NameToThinDocumentMap.
  void ReadDirectoryInternal(const std::string& document_id,
                             const ReadDirectoryInternalCallback& callback);
  void ReadDirectoryInternalWithChildDocuments(
      const ReadDirectoryInternalCallback& callback,
      base::Optional<std::vector<mojom::DocumentPtr>> maybe_children);

  const std::string authority_;
  const std::string root_document_id_;

  // Map from a file path to a watcher data.
  //
  // Note that we do not use a document ID as a key here to guarantee that
  // a watch installed by AddWatcher() can be always identified in
  // RemoveWatcher() with the same file path specified.
  // See the documentation of AddWatcher() for more details.
  std::map<base::FilePath, WatcherData> path_to_watcher_data_;

  uint64_t next_watcher_request_id_ = 1;

  // Can be null if this instance is not observing ArcFileSystemOperationRunner.
  // Observation is started on the first call of AddWatcher().
  scoped_refptr<file_system_operation_runner_util::ObserverIOThreadWrapper>
      observer_wrapper_;

  base::WeakPtrFactory<ArcDocumentsProviderRoot> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderRoot);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
