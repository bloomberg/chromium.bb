// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include <errno.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using content::BrowserThread;

namespace {

const char kGDataRootDirectory[] = "gdata";
const char kFeedField[] = "feed";
const char kWildCard[] = "*";
const FilePath::CharType kGDataCacheVersionDir[] = FILE_PATH_LITERAL("v1");
const FilePath::CharType kGDataCacheBlobsDir[] = FILE_PATH_LITERAL("blobs");
const FilePath::CharType kGDataCacheMetaDir[] = FILE_PATH_LITERAL("meta");
const FilePath::CharType kLastFeedFile[] = FILE_PATH_LITERAL("last_feed.json");

// Internal callback for InitializeCacheOnIOThreadPool which is posted by
// InitializeCache.
typedef base::Callback<void(
    net::Error error,
    bool initialized,
    const gdata::GDataRootDirectory::CacheMap& cache_map)>
        CacheInitializationCallback;

// Internal callback for any task that modifies cache status on IO thread pool,
// e.g. StoreToCacheOnIOThreadPool and ModifyCacheStatusOnIOThreadPool, which
// are posted by StoreToCache and Pin/Unpin respectively.
typedef base::Callback<void(net::Error error,
                            const std::string& res_id,
                            const std::string& md5,
                            mode_t mode_bits,
                            const gdata::CacheOperationCallback& callback)>
    CacheStatusModificationCallback;

// Parameters to pass from StoreToCache on calling thread to
// StoreToCacheOnIOThreadPool task on IO thread pool.
struct StoreToCacheParams {
  StoreToCacheParams(
      const std::string& res_id,
      const std::string& md5,
      const FilePath& source_path,
      const gdata::CacheOperationCallback& operation_callback,
      const FilePath& dest_path,
      const CacheStatusModificationCallback& modification_callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);
  virtual ~StoreToCacheParams();

  const std::string res_id;
  const std::string md5;
  const FilePath source_path;
  const gdata::CacheOperationCallback operation_callback;
  const FilePath dest_path;
  const CacheStatusModificationCallback modification_callback;
  const scoped_refptr<base::MessageLoopProxy> relay_proxy;
};

// Parameters to pass from any method to modify cache status on calling thread
// to ModifyCacheStatusOnIOThreadPool task on IO thread pool.
struct ModifyCacheStatusParams {
  ModifyCacheStatusParams(
      const std::string& res_id,
      const std::string& md5,
      const gdata::CacheOperationCallback& operation_callback,
      gdata::GDataRootDirectory::CacheStatusFlags flags,
      bool enable,
      const FilePath& file_path,
      const CacheStatusModificationCallback& modification_callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);
  virtual ~ModifyCacheStatusParams();

  const std::string res_id;
  const std::string md5;
  const gdata::CacheOperationCallback operation_callback;
  const gdata::GDataRootDirectory::CacheStatusFlags flags;
  const bool enable;
  const FilePath file_path;
  const CacheStatusModificationCallback modification_callback;
  const scoped_refptr<base::MessageLoopProxy> relay_proxy;
};

// Converts gdata error code into file platform error code.
base::PlatformFileError GDataToPlatformError(gdata::GDataErrorCode status) {
  switch (status) {
    case gdata::HTTP_SUCCESS:
    case gdata::HTTP_CREATED:
      return base::PLATFORM_FILE_OK;
    case gdata::HTTP_UNAUTHORIZED:
    case gdata::HTTP_FORBIDDEN:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    case gdata::HTTP_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    case gdata::GDATA_PARSE_ERROR:
    case gdata::GDATA_FILE_ERROR:
      return base::PLATFORM_FILE_ERROR_ABORT;
    default:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
}

//==================== StoreToCacheParams implementations ======================

StoreToCacheParams::StoreToCacheParams(
    const std::string& in_res_id,
    const std::string& in_md5,
    const FilePath& in_source_path,
    const gdata::CacheOperationCallback& in_operation_callback,
    const FilePath& in_dest_path,
    const CacheStatusModificationCallback& in_modification_callback,
    scoped_refptr<base::MessageLoopProxy> in_relay_proxy)
    : res_id(in_res_id),
      md5(in_md5),
      source_path(in_source_path),
      operation_callback(in_operation_callback),
      dest_path(in_dest_path),
      modification_callback(in_modification_callback),
      relay_proxy(in_relay_proxy) {
}

StoreToCacheParams::~StoreToCacheParams() {
}

//=================== ModifyCacheStatusParams implementations ==================

ModifyCacheStatusParams::ModifyCacheStatusParams(
    const std::string& in_res_id,
    const std::string& in_md5,
    const gdata::CacheOperationCallback& in_operation_callback,
    gdata::GDataRootDirectory::CacheStatusFlags in_flags,
    bool in_enable,
    const FilePath& in_file_path,
    const CacheStatusModificationCallback& in_modification_callback,
    scoped_refptr<base::MessageLoopProxy> in_relay_proxy)
    : res_id(in_res_id),
      md5(in_md5),
      operation_callback(in_operation_callback),
      flags(in_flags),
      enable(in_enable),
      file_path(in_file_path),
      modification_callback(in_modification_callback),
      relay_proxy(in_relay_proxy) {
}

ModifyCacheStatusParams::~ModifyCacheStatusParams() {
}

//================================ Helper functions ============================

// Creates cache directory and its sub-directories if they don't exist.
// TODO(glotov): take care of this when the setup and cleanup part is landed,
// noting that these directories need to be created for development in linux box
// and unittest. (http://crosbug.com/27577)
net::Error CreateCacheDirectories(
    const std::vector<FilePath>& paths_to_create) {
  net::Error error = net::OK;

  for (size_t i = 0; i < paths_to_create.size(); ++i) {
    if (file_util::DirectoryExists(paths_to_create[i]))
      continue;

    if (!file_util::CreateDirectory(paths_to_create[i])) {
      // Error creating this directory, record error and proceed with next one.
      error = net::MapSystemError(errno);
      LOG(ERROR) << "Error creating dir " << paths_to_create[i].value()
                 << ": " << net::ErrorToString(error);
    }
  }

  return error;
}

// Modifies RWX attributes of others cateogry of cache file corresponding to
// |res_id| and |md5| on IO thread pool.
// |new_mode_bits| receives the new mode bits of file if modification was
// successful; pass NULL if not interested.
net::Error ModifyCacheFileMode(
    const FilePath& path,
    gdata::GDataRootDirectory::CacheStatusFlags flags,
    bool enable,
    mode_t* new_mode_bits) {
  // Get stat of |path|.
  struct stat64 stat_buf;
  int rv = stat64(path.value().c_str(), &stat_buf);

  // Map system error to net error.
  net::Error error = net::MapSystemError(rv == 0 ? rv : errno);
  if (error != net::OK) {
    DVLOG(1) << "Error getting file info for " << path.value()
             << ": \"" << strerror(errno)
             << "\", " << net::ErrorToString(error);
    return error;
  }

  mode_t updated_mode_bits = stat_buf.st_mode;

  // Modify bits accordingly.
  if (enable)
    updated_mode_bits |= flags;
  else
    updated_mode_bits &= ~flags;

  // Change mode of file attributes.
  rv = chmod(path.value().c_str(), updated_mode_bits);

  // Map system error to net error.
  error = net::MapSystemError(rv == 0 ? rv : errno);
  if (error != net::OK) {
    DVLOG(1) << "Error changing file mode for " << path.value()
             << ": \"" << strerror(errno)
             << "\", " << net::ErrorToString(error);
  } else if (new_mode_bits) {
    *new_mode_bits = updated_mode_bits;
  }

  return error;
}

// Deletes stale cache versions of |res_id|, except for |fresh_path| on io
// thread pool.
void DeleteStaleCacheVersions(const FilePath& fresh_path_to_keep) {
  // Delete all stale cached versions of same res_id in |fresh_path_to_keep|,
  // i.e. with filenames "res_id.*".

  FilePath base_name = fresh_path_to_keep.BaseName();
  std::string stale_filenames;
  // FilePath::Extension returns ".", so strip it.
  if (base_name.Extension().substr(1) != kWildCard) {
    stale_filenames = base_name.RemoveExtension().value() +
                      FilePath::kExtensionSeparator +
                      kWildCard;
  } else {
    stale_filenames = base_name.value();
  }

  // Enumerate all files with res_id.*.
  bool success = true;
  file_util::FileEnumerator traversal(fresh_path_to_keep.DirName(), false,
                                      file_util::FileEnumerator::FILES,
                                      stale_filenames);
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // If current is |fresh_path_to_keep|, don't delete it.
    if (current == fresh_path_to_keep)
      continue;

    success = unlink(current.value().c_str()) == 0;
    if (!success)
      DVLOG(1) << "Error deleting stale " << current.value();
    else
      DVLOG(1) << "Deleted stale " << current.value();
  }
}

//========== Tasks posted from calling thread to run on IO thread pool =========

// Task posted from InitializeCache to run on IO thread pool.
// Creates cache directory and its sub-directories if they don't exist,
// or scans blobs sub-directory for files and their attributes and updates the
// info into cache map.
// Upon completion, OnCacheInitialized is invoked on the thread where
// InitializeCache was called.
void InitializeCacheOnIOThreadPool(
    const std::vector<FilePath>& paths_to_create,
    const FilePath& path_to_scan,
    const CacheInitializationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  bool initialized = false;
  gdata::GDataRootDirectory::CacheMap cache_map;

  net::Error error = CreateCacheDirectories(paths_to_create);

  if (error == net::OK) {
    // Scan cache directory to enumerate all files in it, retrieve their file
    // attributes and creates corresponding entries for cache map.
    file_util::FileEnumerator traversal(path_to_scan, false,
                                        file_util::FileEnumerator::FILES,
                                        kWildCard);
    for (FilePath current = traversal.Next(); !current.empty();
         current = traversal.Next()) {
      // Extract res_id and md5 from filename.
      FilePath base_name = current.BaseName();
      // FilePath::Extension returns ".", so strip it.
      std::string md5 = gdata::GDataFileBase::UnescapeUtf8FileName(
          base_name.Extension().substr(1));
      std::string res_id = gdata::GDataFileBase::UnescapeUtf8FileName(
          base_name.RemoveExtension().value());

      // Retrieve mode bits of file.
      file_util::FileEnumerator::FindInfo info;
      traversal.GetFindInfo(&info);
      mode_t mode_bits = info.stat.st_mode;

      // Insert a CacheEntry for current cached file into ResourceMap.
      gdata::GDataRootDirectory::CacheEntry* entry =
          new gdata::GDataRootDirectory::CacheEntry(md5, mode_bits);
      cache_map.insert(std::make_pair(res_id, entry));
    }
  }

  initialized = true;

  // Invoke callback.
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback, error, initialized, cache_map));
}

// Task posted from StoreToCache to do the following on IO thread pool:
// - moves |source_path| to |dest_path| in the cache directory.
// - sets the appropriate file attributes
// - deletes stale cached versions of |res_id|.
// Upon completion, OnStoredToCache (i.e. |modification_callback|) is invoked on
// the thread where StoreToCache was called.
void StoreToCacheOnIOThreadPool(const StoreToCacheParams& params) {
  net::Error error = net::OK;
  mode_t mode_bits = 0;

  // If |source_path| and |dest_path| are different, move |source_path| to
  // |dest_path| in cache dir.
  if (params.source_path != params.dest_path) {
    if (!file_util::Move(params.source_path, params.dest_path)) {
      error = net::MapSystemError(errno);
      DVLOG(1) << "Error moving " << params.source_path.value()
               << " to " << params.dest_path.value()
               << ": \"" << strerror(errno)
               << "\", " << net::ErrorToString(error);
    }
  }

  if (error == net::OK) {
    // Set cache_ok bit.
    error = ModifyCacheFileMode(params.dest_path,
                                gdata::GDataRootDirectory::CACHE_OK,
                                true,
                                &mode_bits);

    // Delete stale versions of res_id.*.
    DeleteStaleCacheVersions(params.dest_path);
  }

  // Invoke |modification_callback|.
  params.relay_proxy->PostTask(FROM_HERE,
                               base::Bind(params.modification_callback,
                                          error,
                                          params.res_id,
                                          params.md5,
                                          mode_bits,
                                          params.operation_callback));
}

// Task posted from RemoveFromCache to delete stale cache versions corresponding
// to |res_id| on the IO thread pool.
// Upon completion, |callback| is invoked on the thread where RemoveFromCache
// was called.
void DeleteStaleCacheVersionsWithCallback(
    const std::string& res_id,
    const gdata::CacheOperationCallback& callback,
    const FilePath& files_to_delete,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  // We're already on IO thread pool, simply call the actual method.
  DeleteStaleCacheVersions(files_to_delete);

  // Invoke callback on calling thread.
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback, net::OK, res_id, std::string()));
}

// Task posted from Pin and Unpin to modify cache status on the IO thread pool.
// Upon completion, OnCacheStatusModified (i.e. |modification_callback| is
// invoked on the same thread where Pin/Unpin was called.
void ModifyCacheStatusOnIOThreadPool(const ModifyCacheStatusParams& params) {
  // Set or clear bits specified in |flags|.
  mode_t mode_bits = 0;
  net::Error error = ModifyCacheFileMode(params.file_path, params.flags,
                                         params.enable, &mode_bits);

  // Invoke |modification_callback|.
  params.relay_proxy->PostTask(FROM_HERE,
                               base::Bind(params.modification_callback,
                                          error,
                                          params.res_id,
                                          params.md5,
                                          mode_bits,
                                          params.operation_callback));
}

}  // namespace

namespace gdata {

// FindFileDelegate class implementation.

FindFileDelegate::~FindFileDelegate() {
}

// ReadOnlyFindFileDelegate class implementation.

ReadOnlyFindFileDelegate::ReadOnlyFindFileDelegate() : file_(NULL) {
}

void ReadOnlyFindFileDelegate::OnFileFound(gdata::GDataFile* file) {
  // file_ should be set only once since OnFileFound() is a terminal
  // function.
  DCHECK(!file_);
  DCHECK(!file->file_info().is_directory);
  file_ = file;
}

void ReadOnlyFindFileDelegate::OnDirectoryFound(const FilePath&,
                                                GDataDirectory* dir) {
  // file_ should be set only once since OnDirectoryFound() is a terminal
  // function.
  DCHECK(!file_);
  DCHECK(dir->file_info().is_directory);
  file_ = dir;
}

FindFileDelegate::FindFileTraversalCommand
ReadOnlyFindFileDelegate::OnEnterDirectory(const FilePath&, GDataDirectory*) {
  // Keep traversing while doing read only lookups.
  return FIND_FILE_CONTINUES;
}

void ReadOnlyFindFileDelegate::OnError(base::PlatformFileError) {
  file_ = NULL;
}

// FindFileDelegateReplyBase class implementation.

FindFileDelegateReplyBase::FindFileDelegateReplyBase(
      GDataFileSystem* file_system,
      const FilePath& search_file_path,
      bool require_content)
      : file_system_(file_system),
        search_file_path_(search_file_path),
        require_content_(require_content) {
  reply_message_proxy_ = base::MessageLoopProxy::current();
}

FindFileDelegateReplyBase::~FindFileDelegateReplyBase() {
}

FindFileDelegate::FindFileTraversalCommand
FindFileDelegateReplyBase::OnEnterDirectory(
      const FilePath& current_directory_path,
      GDataDirectory* current_dir) {
  return CheckAndRefreshContent(current_directory_path, current_dir) ?
             FIND_FILE_CONTINUES : FIND_FILE_TERMINATES;
}

// Checks if the content of the |directory| under |directory_path| needs to be
// refreshed. Returns true if directory content is fresh, otherwise it kicks
// off content request request. After feed content content is received and
// processed in GDataFileSystem::OnGetDocuments(), that function will also
// restart the initiated FindFileByPath() request.
bool FindFileDelegateReplyBase::CheckAndRefreshContent(
    const FilePath& directory_path, GDataDirectory* directory) {
  GURL feed_url;
  if (directory->NeedsRefresh(&feed_url)) {
    // If content is stale/non-existing, first fetch the content of the
    // directory in order to traverse it further.
    file_system_->RefreshDirectoryAndContinueSearch(
        GDataFileSystem::FindFileParams(
            search_file_path_,
            require_content_,
            directory_path,
            feed_url,
            true,    /* is_initial_feed */
            this));
    return false;
  }
  return true;
}

// GDataFileSystem::FindFileParams struct implementation.

GDataFileSystem::FindFileParams::FindFileParams(
    const FilePath& in_file_path,
    bool in_require_content,
    const FilePath& in_directory_path,
    const GURL& in_feed_url,
    bool in_initial_feed,
    scoped_refptr<FindFileDelegate> in_delegate)
    : file_path(in_file_path),
      require_content(in_require_content),
      directory_path(in_directory_path),
      feed_url(in_feed_url),
      initial_feed(in_initial_feed),
      delegate(in_delegate) {
}

GDataFileSystem::FindFileParams::~FindFileParams() {
}

// GDataFileSystem::CreateDirectoryParams struct implementation.

GDataFileSystem::CreateDirectoryParams::CreateDirectoryParams(
    const FilePath& created_directory_path,
    const FilePath& target_directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback)
    : created_directory_path(created_directory_path),
      target_directory_path(target_directory_path),
      is_exclusive(is_exclusive),
      is_recursive(is_recursive),
      callback(callback) {
}

GDataFileSystem::CreateDirectoryParams::~CreateDirectoryParams() {
}


// GDataFileSystem class implementatsion.

GDataFileSystem::GDataFileSystem(Profile* profile,
                                 DocumentsServiceInterface* documents_service)
    : profile_(profile),
      documents_service_(documents_service),
      gdata_uploader_(new GDataUploader(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      gdata_download_observer_(new GDataDownloadObserver()),
      cache_initialized_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  documents_service_->Initialize(profile_);

  // download_manager will be NULL for unit tests.
  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        DownloadServiceFactory::GetForProfile(profile)->GetDownloadManager() :
        NULL;
  gdata_download_observer_->Initialize(gdata_uploader_.get(), download_manager);
  root_.reset(new GDataRootDirectory());
  root_->set_file_name(kGDataRootDirectory);

  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  gdata_cache_path_ = cache_base_path.Append(chrome::kGDataCacheDirname);
  gdata_cache_path_ = gdata_cache_path_.Append(kGDataCacheVersionDir);
  // Insert into |cache_paths_| in the order defined in enum CacheType.
  cache_paths_.push_back(gdata_cache_path_.Append(kGDataCacheBlobsDir));
  cache_paths_.push_back(gdata_cache_path_.Append(kGDataCacheMetaDir));
  DVLOG(1) << "GCache dirs: blobs=" << cache_paths_[CACHE_TYPE_BLOBS].value()
           << ", meta=" << cache_paths_[CACHE_TYPE_META].value();
  InitializeCache();
}

GDataFileSystem::~GDataFileSystem() {
  // Should be deleted as part of Profile on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void GDataFileSystem::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Cancel all the in-flight operations.
  // This asynchronously cancels the URL fetch operations.
  documents_service_->CancelAll();
}

void GDataFileSystem::Authenticate(const AuthStatusCallback& callback) {
  // TokenFetcher, used in DocumentsService, must be run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_->Authenticate(callback);
}

void GDataFileSystem::FindFileByPath(
    const FilePath& file_path, scoped_refptr<FindFileDelegate> delegate) {
  base::AutoLock lock(lock_);
  UnsafeFindFileByPath(file_path, delegate);
}

void GDataFileSystem::RefreshDirectoryAndContinueSearch(
    const FindFileParams& params) {
  scoped_ptr<base::ListValue> feed_list(new base::ListValue());
  // Kick off document feed fetching here if we don't have complete data
  // to finish this call.
  // |feed_list| will contain the list of all collected feed updates that
  // we will receive through calls of DocumentsService::GetDocuments().
  ContinueDirectoryRefresh(params, feed_list.Pass());
}

void GDataFileSystem::Remove(const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  base::AutoLock lock(lock_);
  GDataFileBase* file_info = GetGDataFileInfoFromPath(file_path);
  if (!file_info) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    }
    return;
  }

  documents_service_->DeleteDocument(
      file_info->self_url(),
      base::Bind(&GDataFileSystem::OnRemovedDocument,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 file_path));
}

void GDataFileSystem::CreateDirectory(
    const FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  FilePath last_parent_dir_path;
  FilePath first_missing_path;
  GURL last_parent_dir_url;
  FindMissingDirectoryResult result =
      FindFirstMissingParentDirectory(directory_path,
                                      &last_parent_dir_url,
                                      &first_missing_path);
  switch (result) {
    case FOUND_INVALID: {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
      }

      return;
    }
    case DIRECTORY_ALREADY_PRESENT: {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback,
                       is_exclusive ? base::PLATFORM_FILE_ERROR_EXISTS :
                                      base::PLATFORM_FILE_OK));
      }

      return;
    }
    case FOUND_MISSING: {
      // There is a missing folder to be created here, move on with the rest of
      // this function.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  // Do we have a parent directory here as well? We can't then create target
  // directory if this is not a recursive operation.
  if (directory_path !=  first_missing_path && !is_recursive) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
           base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    }
    return;
  }

  documents_service_->CreateDirectory(
      last_parent_dir_url,
      first_missing_path.BaseName().value(),
      base::Bind(&GDataFileSystem::OnCreateDirectoryCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 CreateDirectoryParams(
                     first_missing_path,
                     directory_path,
                     is_exclusive,
                     is_recursive,
                     callback)));
}

void GDataFileSystem::GetFile(const FilePath& file_path,
                              const GetFileCallback& callback) {
  base::AutoLock lock(lock_);
  GDataFileBase* file_info = GetGDataFileInfoFromPath(file_path);
  if (!file_info) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     base::PLATFORM_FILE_ERROR_NOT_FOUND,
                     FilePath()));
    }
    return;
  }

  // TODO(satorux): We should get a file from the cache if it's present, but
  // the caching layer is not implemented yet. For now, always download from
  // the cloud.
  documents_service_->DownloadFile(
      file_info->content_url(),
      base::Bind(&GDataFileSystem::OnFileDownloaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void GDataFileSystem::InitiateUpload(
    const std::string& file_name,
    const std::string& content_type,
    int64 content_length,
    const FilePath& destination_directory,
    const InitiateUploadOperationCallback& callback) {
  GURL destination_directory_url =
      GetUploadUrlForDirectory(destination_directory);

  if (destination_directory_url.is_empty()) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     HTTP_BAD_REQUEST, GURL()));
    }
    return;
  }

  documents_service_->InitiateUpload(
          InitiateUploadParams(file_name,
                                 content_type,
                                 content_length,
                                 destination_directory_url),
          base::Bind(&GDataFileSystem::OnUploadLocationReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     // MessageLoopProxy is used to run |callback| on the
                     // thread where this function was called.
                     base::MessageLoopProxy::current()));
}

void GDataFileSystem::OnUploadLocationReceived(
    const InitiateUploadOperationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    GDataErrorCode code,
    const GURL& upload_location) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null()) {
    message_loop_proxy->PostTask(FROM_HERE, base::Bind(callback, code,
                                                       upload_location));
  }
}

void GDataFileSystem::ResumeUpload(
    const ResumeUploadParams& params,
    const ResumeUploadOperationCallback& callback) {
  documents_service_->ResumeUpload(
          params,
          base::Bind(&GDataFileSystem::OnResumeUpload,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     // MessageLoopProxy is used to run |callback| on the
                     // thread where this function was called.
                     base::MessageLoopProxy::current()));
}

void GDataFileSystem::OnResumeUpload(
    const ResumeUploadOperationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null()) {
    message_loop_proxy->PostTask(
        FROM_HERE,
        base::Bind(callback, code, start_range_received, end_range_received));
  }
  // TODO(achuith): Figure out when we are done with upload and
  // add appropriate entry to the file system that represents the new file.
}


void GDataFileSystem::UnsafeFindFileByPath(
    const FilePath& file_path, scoped_refptr<FindFileDelegate> delegate) {
  lock_.AssertAcquired();

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);

  GDataDirectory* current_dir = root_.get();
  FilePath directory_path;
  for (size_t i = 0; i < components.size() && current_dir; i++) {
    directory_path = directory_path.Append(current_dir->file_name());

    // Last element must match, if not last then it must be a directory.
    if (i == components.size() - 1) {
      if (current_dir->file_name() == components[i])
        delegate->OnDirectoryFound(directory_path, current_dir);
      else
        delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return;
    }

    if (delegate->OnEnterDirectory(directory_path, current_dir) ==
        FindFileDelegate::FIND_FILE_TERMINATES) {
      return;
    }

    // Not the last part of the path, search for the next segment.
    GDataFileCollection::const_iterator file_iter =
        current_dir->children().find(components[i + 1]);
    if (file_iter == current_dir->children().end()) {
      delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }

    // Found file, must be the last segment.
    if (file_iter->second->file_info().is_directory) {
      // Found directory, continue traversal.
      current_dir = file_iter->second->AsGDataDirectory();
    } else {
      if ((i + 1) == (components.size() - 1))
        delegate->OnFileFound(file_iter->second->AsGDataFile());
      else
        delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return;
    }
  }
  delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
}

GDataFileBase* GDataFileSystem::GetGDataFileInfoFromPath(
    const FilePath& file_path) {
  lock_.AssertAcquired();
  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, find_delegate);
  if (!find_delegate->file())
    return NULL;

  return find_delegate->file();
}

FilePath GDataFileSystem::GetFromCacheForPath(const FilePath& gdata_file_path) {
  FilePath cache_file_path;
  std::string resource;
  std::string md5;

  {  // Lock to use GetGDataFileInfoFromPath and returned pointer, but need to
     // release before GetFromCache.
    base::AutoLock lock(lock_);
    GDataFileBase* file_base = GetGDataFileInfoFromPath(gdata_file_path);

    if (!file_base || !file_base->AsGDataFile())
      return cache_file_path;

    GDataFile* file = file_base->AsGDataFile();
    resource = file->resource_id();
    md5 = file->file_md5();
  }

  if (!GetFromCache(resource, md5, &cache_file_path))
    cache_file_path.clear();

  return cache_file_path;
}

void GDataFileSystem::OnCreateDirectoryCompleted(
    const CreateDirectoryParams& params,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {

  base::PlatformFileError error = GDataToPlatformError(status);
  if (error != base::PLATFORM_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(error);

    return;
  }

  base::DictionaryValue* dict_value = NULL;
  base::Value* created_entry = NULL;
  if (data.get() && data->GetAsDictionary(&dict_value) && dict_value)
    dict_value->Get("entry", &created_entry);
  error = AddNewDirectory(params.created_directory_path, created_entry);

  if (error != base::PLATFORM_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(error);

    return;
  }

  // Not done yet with recursive directory creation?
  if (params.target_directory_path != params.created_directory_path &&
      params.is_recursive) {
    CreateDirectory(params.target_directory_path,
                    params.is_exclusive,
                    params.is_recursive,
                    params.callback);
    return;
  }

  if (!params.callback.is_null()) {
    // Finally done with the create request.
    params.callback.Run(base::PLATFORM_FILE_OK);
  }
}

void GDataFileSystem::ContinueDirectoryRefresh(
    const FindFileParams& params, scoped_ptr<base::ListValue> feed_list) {
  // Kick off document feed fetching here if we don't have complete data
  // to finish this call.
  documents_service_->GetDocuments(
      params.feed_url,
      base::Bind(&GDataFileSystem::OnGetDocuments,
                 weak_ptr_factory_.GetWeakPtr(),
                 params,
                 base::Passed(&feed_list)));
}


void GDataFileSystem::OnGetDocuments(
    const FindFileParams& params,
    scoped_ptr<base::ListValue> feed_list,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {

  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "No feed content!";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  // TODO(zelidrag): Find a faster way to get next url rather than parsing
  // the entire feed.
  GURL next_feed_url;
  scoped_ptr<DocumentFeed> current_feed(ParseDocumentFeed(data.get()));
  if (!current_feed.get()) {
    params.delegate->OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }

  // Add the current feed to the list of collected feeds for this directory.
  feed_list->Append(data.release());

  // Check if we need to collect more data to complete the directory list.
  if (current_feed->GetNextFeedURL(&next_feed_url) &&
      !next_feed_url.is_empty()) {
    // Kick of the remaining part of the feeds.
    ContinueDirectoryRefresh(FindFileParams(params.file_path,
                                            params.require_content,
                                            params.directory_path,
                                            next_feed_url,
                                            false,  /* initial_feed */
                                            params.delegate),
                             feed_list.Pass());
    return;
  }

  error = UpdateDirectoryWithDocumentFeed(params.directory_path,
                                          feed_list.get());
  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  // If this was the root feed, cache its content.
  if (params.directory_path == FilePath(kGDataRootDirectory))
    SaveRootFeeds(feed_list.Pass());

  // Continue file content search operation.
  FindFileByPath(params.file_path,
                 params.delegate);
}

void GDataFileSystem::SaveRootFeeds(scoped_ptr<base::ListValue> feed_vector) {
  BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&GDataFileSystem::SaveRootFeedsOnIOThreadPool,
                 cache_paths_[CACHE_TYPE_META],
                 base::Passed(&feed_vector)));
}

// Static.
void GDataFileSystem::SaveRootFeedsOnIOThreadPool(
    const FilePath& meta_cache_path,
    scoped_ptr<base::ListValue> feed_vector) {
  if (!file_util::DirectoryExists(meta_cache_path)) {
    if (!file_util::CreateDirectory(meta_cache_path)) {
      LOG(WARNING) << "GData metadata cache directory can't be created at "
                   << meta_cache_path.value();
      return;
    }
  }

  FilePath file_name = meta_cache_path.Append(kLastFeedFile);
  std::string json;
  base::JSONWriter::Write(feed_vector.get(),
                          false,   // pretty_print
                          &json);

  int file_size = static_cast<int>(json.length());
  if (file_util::WriteFile(file_name, json.data(), file_size) != file_size) {
    LOG(WARNING) << "GData metadata file can't be stored at "
                 << file_name.value();
    if (!file_util::Delete(file_name, true)) {
      LOG(WARNING) << "GData metadata file can't be deleted at "
                   << file_name.value();
      return;
    }
  }
}

void GDataFileSystem::OnRemovedDocument(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    GDataErrorCode status,
    const GURL& document_url) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK)
    error = RemoveFileFromFileSystem(file_path);

  if (!callback.is_null()) {
    callback.Run(error);
  }
}

void GDataFileSystem::OnFileDownloaded(
    const GetFileCallback& callback,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& file_path) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (!callback.is_null()) {
    callback.Run(error, file_path);
  }
}

base::PlatformFileError GDataFileSystem::RemoveFileFromFileSystem(
    const FilePath& file_path) {
  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, update_delegate);

  GDataFileBase* file = update_delegate->file();

  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // If it's a file (only files have resource), remove it from cache.
  if (file->AsGDataFile()) {
    RemoveFromCache(file->AsGDataFile()->resource_id(),
                    base::Bind(&GDataFileSystem::OnRemovedFromCache,
                               weak_ptr_factory_.GetWeakPtr()));
  }

  // You can't remove root element.
  if (!file->parent())
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

  if (!file->parent()->RemoveFile(file))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  return base::PLATFORM_FILE_OK;
}

DocumentFeed* GDataFileSystem::ParseDocumentFeed(base::Value* feed_data) {
  DCHECK(feed_data->IsType(Value::TYPE_DICTIONARY));
  base::DictionaryValue* feed_dict = NULL;
  if (!static_cast<base::DictionaryValue*>(feed_data)->GetDictionary(
          kFeedField, &feed_dict)) {
    return NULL;
  }
  // Parse the document feed.
  return DocumentFeed::CreateFrom(feed_dict);
}


base::PlatformFileError GDataFileSystem::UpdateDirectoryWithDocumentFeed(
    const FilePath& directory_path,
    base::ListValue* feed_list) {
  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(directory_path, update_delegate);

  GDataFileBase* file = update_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  GDataDirectory* dir = file->AsGDataDirectory();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

  dir->set_refresh_time(base::Time::Now());

  // Remove all child elements if we are refreshing the entire content.
  dir->RemoveChildren();

  bool first_feed = true;
  // Get through all collected feeds and fill in directory structure for all
  // of them.
  for (base::ListValue::const_iterator feeds_iter = feed_list->begin();
       feeds_iter != feed_list->end(); ++feeds_iter) {
    scoped_ptr<DocumentFeed> feed(ParseDocumentFeed(*feeds_iter));
    if (!feed.get())
      return base::PLATFORM_FILE_ERROR_FAILED;

    // Get upload url from the root feed. Links for all other collections will
    // be handled in GDatadirectory::FromDocumentEntry();
    if (dir == root_.get() && first_feed) {
      const Link* root_feed_upload_link =
          feed->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
      if (root_feed_upload_link)
        dir->set_upload_url(root_feed_upload_link->href());
    }

    first_feed = false;

    for (ScopedVector<DocumentEntry>::const_iterator iter =
             feed->entries().begin();
         iter != feed->entries().end(); ++iter) {
      DocumentEntry* doc = *iter;

      // For now, skip elements of the root directory feed that have parent.
      // TODO(oleg): http://crosbug.com/26908. In theory, we could reconstruct
      // the entire FS snapshot instead of fetching one dir/collection at
      // the time.
      if (dir == root_.get()) {
        const Link* parent_link = doc->GetLinkByType(Link::PARENT);
        if (parent_link)
          continue;
      }

      GDataFileBase* file = GDataFileBase::FromDocumentEntry(dir, doc);
      if (file)
        dir->AddFile(file);
    }
  }

  NotifyDirectoryChanged(directory_path);

  return base::PLATFORM_FILE_OK;
}

void GDataFileSystem::NotifyDirectoryChanged(const FilePath& directory_path) {
  // TODO(zelidrag): Notify all observers on directory content change here.
}

base::PlatformFileError GDataFileSystem::AddNewDirectory(
    const FilePath& directory_path, base::Value* entry_value) {
  if (!entry_value)
    return base::PLATFORM_FILE_ERROR_FAILED;

  scoped_ptr<DocumentEntry> entry(DocumentEntry::CreateFrom(entry_value));

  if (!entry.get())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find parent directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(directory_path.DirName(), update_delegate);

  GDataFileBase* file = update_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // Check if parent is a directory since in theory since this is a callback
  // something could in the meantime have nuked the parent dir and created a
  // file with the exact same name.
  GDataDirectory* parent_dir = file->AsGDataDirectory();
  if (!parent_dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

  GDataFileBase* new_file = GDataFileBase::FromDocumentEntry(parent_dir,
                                                             entry.get());
  if (!new_file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  parent_dir->AddFile(new_file);

  return base::PLATFORM_FILE_OK;
}

GDataFileSystem::FindMissingDirectoryResult
GDataFileSystem::FindFirstMissingParentDirectory(
    const FilePath& directory_path,
    GURL* last_dir_content_url,
    FilePath* first_missing_parent_path) {
  // Let's find which how deep is the existing directory structure and
  // get the first element that's missing.
  std::vector<FilePath::StringType> path_parts;
  directory_path.GetComponents(&path_parts);
  FilePath current_path;

  base::AutoLock lock(lock_);
  for (std::vector<FilePath::StringType>::const_iterator iter =
          path_parts.begin();
       iter != path_parts.end(); ++iter) {
    current_path = current_path.Append(*iter);
    scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
        new ReadOnlyFindFileDelegate());
    UnsafeFindFileByPath(current_path, find_delegate);
    if (find_delegate->file()) {
      if (find_delegate->file()->file_info().is_directory) {
        *last_dir_content_url = find_delegate->file()->content_url();
      } else {
        // Huh, the segment found is a file not a directory?
        return FOUND_INVALID;
      }
    } else {
      *first_missing_parent_path = current_path;
      return FOUND_MISSING;
    }
  }
  return DIRECTORY_ALREADY_PRESENT;
}

GURL GDataFileSystem::GetUploadUrlForDirectory(
    const FilePath& destination_directory) {
  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
      new ReadOnlyFindFileDelegate());
  base::AutoLock lock(lock_);
  UnsafeFindFileByPath(destination_directory, find_delegate);
  GDataDirectory* dir = find_delegate->file() ?
      find_delegate->file()->AsGDataDirectory() : NULL;
  return dir ? dir->upload_url() : GURL();
}

//===================== GDataFileSystem: Cache entry points ====================

FilePath GDataFileSystem::GetCacheFilePath(const std::string& res_id,
                                           const std::string& md5) {
  // Runs on any thread.
  // Filename is formatted as res_id.md5, i.e. res_id is the base name and
  // md5 is the extension.
  FilePath path(GDataFileBase::EscapeUtf8FileName(res_id) +
                FilePath::kExtensionSeparator +
                GDataFileBase::EscapeUtf8FileName(md5));
  return cache_paths_[CACHE_TYPE_BLOBS].Append(path);
}

void GDataFileSystem::InitializeCache() {
  // Lock to access cache_initialized_;
  base::AutoLock lock(lock_);
  if (cache_initialized_)
    return;

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(InitializeCacheOnIOThreadPool,
                 cache_paths_,
                 cache_paths_[CACHE_TYPE_BLOBS],
                 base::Bind(&GDataFileSystem::OnCacheInitialized,
                            weak_ptr_factory_.GetWeakPtr()),
                 base::MessageLoopProxy::current()));
}

bool GDataFileSystem::StoreToCache(const std::string& res_id,
                                   const std::string& md5,
                                   const FilePath& source_path,
                                   const CacheOperationCallback& callback) {
  if (!SafeIsCacheInitialized())
    return false;

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(StoreToCacheOnIOThreadPool,
                 StoreToCacheParams(
                     res_id,
                     md5,
                     source_path,
                     callback,
                     GetCacheFilePath(res_id, md5),
                     base::Bind(&GDataFileSystem::OnStoredToCache,
                                weak_ptr_factory_.GetWeakPtr()),
                     base::MessageLoopProxy::current())));

  return true;
}

bool GDataFileSystem::GetFromCache(const std::string& res_id,
                                   const std::string& md5,
                                   FilePath* path) {
  // Lock to read cache_initialized_ and access cache map.
  base::AutoLock lock(lock_);
  if (!cache_initialized_)
    return false;

  if (root_->CacheFileExists(res_id, md5)) {
    *path = GetCacheFilePath(res_id, md5);
  } else {
    path->clear();
  }

  return true;
}

bool GDataFileSystem::RemoveFromCache(const std::string& res_id,
                                      const CacheOperationCallback& callback) {
  lock_.AssertAcquired();

  root_->RemoveFromCacheMap(res_id);

  // Post task to delete all cache versions of res_id.
  // If $res_id.0 is passed to DeleteStaleCacheVersions, then all $res_id.* will
  // be deleted since no file will match "$res_id.0".
  FilePath files_to_delete = GetCacheFilePath(res_id, kWildCard);
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(DeleteStaleCacheVersionsWithCallback,
                 res_id,
                 callback,
                 files_to_delete,
                 base::MessageLoopProxy::current()));

  return true;
}

bool GDataFileSystem::Pin(const std::string& res_id,
                          const std::string& md5,
                          const CacheOperationCallback& callback) {
  if (!SafeIsCacheInitialized())
    return false;

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(ModifyCacheStatusOnIOThreadPool,
                 ModifyCacheStatusParams(
                     res_id,
                     md5,
                     callback,
                     GDataRootDirectory::CACHE_PINNED,
                     true,
                     GetCacheFilePath(res_id, md5),
                     base::Bind(&GDataFileSystem::OnCacheStatusModified,
                                weak_ptr_factory_.GetWeakPtr()),
                     base::MessageLoopProxy::current())));

  return true;
}

bool GDataFileSystem::Unpin(const std::string& res_id,
                            const std::string& md5,
                            const CacheOperationCallback& callback) {
  if (!SafeIsCacheInitialized())
    return false;

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(ModifyCacheStatusOnIOThreadPool,
                 ModifyCacheStatusParams(
                     res_id,
                     md5,
                     callback,
                     GDataRootDirectory::CACHE_PINNED,
                     false,
                     GetCacheFilePath(res_id, md5),
                     base::Bind(&GDataFileSystem::OnCacheStatusModified,
                                weak_ptr_factory_.GetWeakPtr()),
                     base::MessageLoopProxy::current())));

  return true;
}

//=== GDataFileSystem: Cache callbacks for tasks that ran on io thread pool ====

void GDataFileSystem::OnCacheInitialized(
    net::Error error,
    bool initialized,
    const GDataRootDirectory::CacheMap& cache_map) {
  DVLOG(1) << "OnCacheInitialized: " << net::ErrorToString(error);

  // Lock to update cache_initailized_ and cache_map_.
  base::AutoLock lock(lock_);
  cache_initialized_ = initialized;
  root_->SetCacheMap(cache_map);
}

void GDataFileSystem::OnStoredToCache(net::Error error,
                                      const std::string& res_id,
                                      const std::string& md5,
                                      mode_t mode_bits,
                                      const CacheOperationCallback& callback) {
  DVLOG(1) << "OnStoredToCache: " << net::ErrorToString(error);

  if (error != net::OK)
    return;

  // Lock to update cache map.
  base::AutoLock lock(lock_);
  root_->UpdateCacheMap(res_id, md5, mode_bits);

  // Invoke callback.
  if (!callback.is_null()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, error, res_id, md5));
  }
}

void GDataFileSystem::OnRemovedFromCache(net::Error error,
                                         const std::string& res_id,
                                         const std::string& md5) {
  DVLOG(1) << "OnRemovedFromCache: " << net::ErrorToString(error);
}

void GDataFileSystem::OnCacheStatusModified(
    net::Error error,
    const std::string& res_id,
    const std::string& md5,
    mode_t mode_bits,
    const CacheOperationCallback& callback) {
  DVLOG(1) << "OnCacheStatusModified: " << net::ErrorToString(error);

  if (error != net::OK)
    return;

  // Lock to update cache map.
  base::AutoLock lock(lock_);
  root_->UpdateCacheMap(res_id, md5, mode_bits);

  // Invoke callback.
  if (!callback.is_null()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, error, res_id, md5));
  }
}

//============ GDataFileSystem: Cache internal helper functions ================

bool GDataFileSystem::SafeIsCacheInitialized() {
  base::AutoLock lock(lock_);
  return cache_initialized_;
}

//========================= GDataFileSystemFactory =============================

// static
GDataFileSystem* GDataFileSystemFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GDataFileSystem*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GDataFileSystemFactory* GDataFileSystemFactory::GetInstance() {
  return Singleton<GDataFileSystemFactory>::get();
}

GDataFileSystemFactory::GDataFileSystemFactory()
    : ProfileKeyedServiceFactory("GDataFileSystem",
                                 ProfileDependencyManager::GetInstance()) {
}

GDataFileSystemFactory::~GDataFileSystemFactory() {
}

ProfileKeyedService* GDataFileSystemFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new GDataFileSystem(profile, new DocumentsService);
}

}  // namespace gdata
