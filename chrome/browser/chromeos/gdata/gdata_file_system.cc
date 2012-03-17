// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include <errno.h>

#include "base/bind.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using content::BrowserThread;

namespace {

const FilePath::CharType kGDataRootDirectory[] = FILE_PATH_LITERAL("gdata");
const char kFeedField[] = "feed";
const char kWildCard[] = "*";
const FilePath::CharType kGDataCacheVersionDir[] = FILE_PATH_LITERAL("v1");
const FilePath::CharType kGDataCacheBlobsDir[] = FILE_PATH_LITERAL("blobs");
const FilePath::CharType kGDataCacheMetaDir[] = FILE_PATH_LITERAL("meta");
const FilePath::CharType kGDataCacheTmpDir[] = FILE_PATH_LITERAL("tmp");
const FilePath::CharType kLastFeedFile[] = FILE_PATH_LITERAL("last_feed.json");
const char kGDataFileSystemToken[] = "GDataFileSystemToken";
const FilePath::CharType kAccountMetadataFile[] =
    FILE_PATH_LITERAL("account_metadata.json");

// Internal callback for GetFromCache on IO thread pool.
typedef base::Callback<void(const std::string& resource_id,
                            const std::string& md5,
                            const FilePath& gdata_file_path,
                            const gdata::GetFromCacheCallback& callback)>
    GetFromCacheSafelyCallback;

// Internal callback for any task that modifies cache status on IO thread pool,
// e.g. StoreToCacheOnIOThreadPool and ModifyCacheStatusOnIOThreadPool, which
// are posted by StoreToCache and Pin/Unpin respectively.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5,
                            mode_t mode_bits,
                            const gdata::CacheOperationCallback& callback)>
    CacheStatusModificationCallback;

// Internal callback for GetCacheState on IO thread pool.
typedef base::Callback<void(const std::string& resource_id,
                            const std::string& md5,
                            const gdata::GetCacheStateCallback& callback)>
    GetCacheStateSafelyCallback;

// Parameters to pass from StoreToCache on calling thread to
// StoreToCacheOnIOThreadPool task on IO thread pool.
struct StoreToCacheParams {
  StoreToCacheParams(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& source_path,
      const gdata::CacheOperationCallback& operation_callback,
      const FilePath& dest_path,
      const CacheStatusModificationCallback& modification_callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);
  virtual ~StoreToCacheParams();

  const std::string resource_id;
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
      const std::string& resource_id,
      const std::string& md5,
      const gdata::CacheOperationCallback& operation_callback,
      gdata::GDataRootDirectory::CacheStatusFlags flags,
      bool enable,
      const FilePath& file_path,
      const CacheStatusModificationCallback& modification_callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);
  virtual ~ModifyCacheStatusParams();

  const std::string resource_id;
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

// Converts system error to file platform error code.
// This is copied and modified from base/platform_file_posix.cc.
// TODO(kuan): base/platform.h should probably export this.
base::PlatformFileError SystemToPlatformError(int error) {
  switch (error) {
    case 0:
      return base::PLATFORM_FILE_OK;
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    case ETXTBSY:
      return base::PLATFORM_FILE_ERROR_IN_USE;
    case EEXIST:
      return base::PLATFORM_FILE_ERROR_EXISTS;
    case ENOENT:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    case EMFILE:
      return base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED;
    case ENOMEM:
      return base::PLATFORM_FILE_ERROR_NO_MEMORY;
    case ENOSPC:
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    case ENOTDIR:
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    case EINTR:
      return base::PLATFORM_FILE_ERROR_ABORT;
    default:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
}

//==================== StoreToCacheParams implementations ======================

StoreToCacheParams::StoreToCacheParams(
    const std::string& in_resource_id,
    const std::string& in_md5,
    const FilePath& in_source_path,
    const gdata::CacheOperationCallback& in_operation_callback,
    const FilePath& in_dest_path,
    const CacheStatusModificationCallback& in_modification_callback,
    scoped_refptr<base::MessageLoopProxy> in_relay_proxy)
    : resource_id(in_resource_id),
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
    const std::string& in_resource_id,
    const std::string& in_md5,
    const gdata::CacheOperationCallback& in_operation_callback,
    gdata::GDataRootDirectory::CacheStatusFlags in_flags,
    bool in_enable,
    const FilePath& in_file_path,
    const CacheStatusModificationCallback& in_modification_callback,
    scoped_refptr<base::MessageLoopProxy> in_relay_proxy)
    : resource_id(in_resource_id),
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
base::PlatformFileError CreateCacheDirectories(
    const std::vector<FilePath>& paths_to_create) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;

  for (size_t i = 0; i < paths_to_create.size(); ++i) {
    if (file_util::DirectoryExists(paths_to_create[i]))
      continue;

    if (!file_util::CreateDirectory(paths_to_create[i])) {
      // Error creating this directory, record error and proceed with next one.
      error = SystemToPlatformError(errno);
      LOG(ERROR) << "Error creating dir " << paths_to_create[i].value()
                 << ": \"" << strerror(errno)
                 << "\", " << error;
    } else {
      DVLOG(1) << "Created dir " << paths_to_create[i].value();
    }
  }

  return error;
}

// Modifies RWX attributes of others cateogry of cache file corresponding to
// |resource_id| and |md5| on IO thread pool.
// |new_mode_bits| receives the new mode bits of file if modification was
// successful; pass NULL if not interested.
base::PlatformFileError ModifyCacheFileMode(
    const FilePath& path,
    gdata::GDataRootDirectory::CacheStatusFlags flags,
    bool enable,
    mode_t* new_mode_bits) {
  // Get stat of |path|.
  struct stat64 stat_buf;
  int rv = HANDLE_EINTR(stat64(path.value().c_str(), &stat_buf));

  // Map system error to net error.
  if (rv != 0) {
    base::PlatformFileError error = SystemToPlatformError(errno);
    DVLOG(1) << "Error getting file info for " << path.value()
             << ": \"" << strerror(errno)
             << "\", " << error;
    return error;
  }

  mode_t updated_mode_bits = stat_buf.st_mode;

  // Modify bits accordingly.
  if (enable)
    updated_mode_bits |= flags;
  else
    updated_mode_bits &= ~flags;

  // Change mode of file attributes.
  rv = HANDLE_EINTR(chmod(path.value().c_str(), updated_mode_bits));

  // Map system error to net error.
  if (rv != 0) {
    base::PlatformFileError error = SystemToPlatformError(errno);
    DVLOG(1) << "Error changing file mode for " << path.value()
             << ": \"" << strerror(errno)
             << "\", " << error;
    return error;
  }

  if (new_mode_bits)
    *new_mode_bits = updated_mode_bits;

  return base::PLATFORM_FILE_OK;
}

// Deletes stale cache versions of |resource_id|, except for |fresh_path| on io
// thread pool.
void DeleteStaleCacheVersions(const FilePath& fresh_path_to_keep) {
  // Delete all stale cached versions of same resource_id in
  // |fresh_path_to_keep|, i.e. with filenames "resource_id.*".

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

  // Enumerate all files with resource_id.*.
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

// Task posted from StoreToCache to do the following on IO thread pool:
// - moves |source_path| to |dest_path| in the cache directory.
// - sets the appropriate file attributes
// - deletes stale cached versions of |resource_id|.
// Upon completion, OnStoredToCache (i.e. |modification_callback|) is invoked on
// the thread where StoreToCache was called.
void StoreToCacheOnIOThreadPool(const StoreToCacheParams& params) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  mode_t mode_bits = 0;

  // If |source_path| and |dest_path| are different, move |source_path| to
  // |dest_path| in cache dir.
  if (params.source_path != params.dest_path) {
    if (!file_util::Move(params.source_path, params.dest_path)) {
      error = SystemToPlatformError(errno);
      LOG(ERROR) << "Error moving " << params.source_path.value()
                 << " to " << params.dest_path.value()
                 << ": \"" << strerror(errno)
                 << "\", " << error;
    } else {
      DVLOG(1) << "Moved " << params.source_path.value()
               << " to " << params.dest_path.value();
    }
  }

  if (error == base::PLATFORM_FILE_OK) {
    // Set cache_ok bit.
    error = ModifyCacheFileMode(params.dest_path,
                                gdata::GDataRootDirectory::CACHE_OK,
                                true,
                                &mode_bits);

    // Delete stale versions of resource_id.*.
    DeleteStaleCacheVersions(params.dest_path);
  }

  // Invoke |modification_callback|.
  params.relay_proxy->PostTask(FROM_HERE,
                               base::Bind(params.modification_callback,
                                          error,
                                          params.resource_id,
                                          params.md5,
                                          mode_bits,
                                          params.operation_callback));
}

// Task posted from GetFromCacheInternal to run on IO thread pool.
// This is just a pass through to invoke the actual |safe_callback|, posted
// solely to force synchronization of all tasks on io thread pool, specifically
// to make sure that this task executes after InitializeCacheOnIOThreadPool.
// It simply invokes OnGetFromCache i.e. |safe_callback|) on the thread where
// GetFromCache was called.
void GetFromCacheOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& gdata_file_path,
    const gdata::GetFromCacheCallback& operation_callback,
    const GetFromCacheSafelyCallback& safe_callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  // Invoke |_callback|.
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(safe_callback,
                                   resource_id,
                                   md5,
                                   gdata_file_path,
                                   operation_callback));
}

// Task posted from RemoveFromCache to delete stale cache versions corresponding
// to |resource_id| on the IO thread pool.
// Upon completion, |callback| is invoked on the thread where RemoveFromCache
// was called.
void DeleteStaleCacheVersionsWithCallback(
    const std::string& resource_id,
    const gdata::CacheOperationCallback& callback,
    const FilePath& files_to_delete,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  // We're already on IO thread pool, simply call the actual method.
  DeleteStaleCacheVersions(files_to_delete);

  // Invoke callback on calling thread.
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback,
                                   base::PLATFORM_FILE_OK,
                                   resource_id,
                                   std::string()));
}

// Task posted from Pin and Unpin to modify cache status on the IO thread pool.
// Upon completion, OnCacheStatusModified (i.e. |modification_callback| is
// invoked on the same thread where Pin/Unpin was called.
void ModifyCacheStatusOnIOThreadPool(const ModifyCacheStatusParams& params) {
  // Set or clear bits specified in |flags|.
  mode_t mode_bits = 0;
  base::PlatformFileError error = ModifyCacheFileMode(params.file_path,
                                                      params.flags,
                                                      params.enable,
                                                      &mode_bits);

  // Invoke |modification_callback|.
  params.relay_proxy->PostTask(FROM_HERE,
                               base::Bind(params.modification_callback,
                                          error,
                                          params.resource_id,
                                          params.md5,
                                          mode_bits,
                                          params.operation_callback));
}

// Task posted from GetCacheState to run on IO thread pool.
// This is just a pass through to invoke the actual |safe_callback|, posted
// solely to force synchronization of all tasks on io thread pool, specifically
// to make sure that this task executes after InitializeCacheOnIOThreadPool.
// It simply invokes OnGetCacheState i.e. |safe_callback|) on the thread
// where GetCacheState was called.
void GetCacheStateOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    const gdata::GetCacheStateCallback& operation_callback,
    const GetCacheStateSafelyCallback& safe_callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  // Invoke |_callback|.
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(safe_callback,
                                   resource_id,
                                   md5,
                                   operation_callback));
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

bool ReadOnlyFindFileDelegate::had_terminated() const {
  return false;
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
      on_cache_initialized_(new base::WaitableEvent(
          true /* manual reset*/, false /* initially not signaled*/)),
      cache_initialization_started_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  documents_service_->Initialize(profile_);

  // download_manager will be NULL for unit tests.
  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        DownloadServiceFactory::GetForProfile(profile)->GetDownloadManager() :
        NULL;
  gdata_download_observer_->Initialize(gdata_uploader_.get(), download_manager);
  root_.reset(new GDataRootDirectory(this));
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
  cache_paths_.push_back(gdata_cache_path_.Append(kGDataCacheTmpDir));
  DVLOG(1) << "GCache dir: " << gdata_cache_path_.value();
}

GDataFileSystem::~GDataFileSystem() {
  // Should be deleted as part of Profile on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // In the rare event that InitializeCacheOnIOThreadPool may not have
  // completed, wait for its completion before destructing because it accesses
  // data members.

  bool need_to_wait = false;
  {  // Lock to access cache_initialization_started_, but need to release
     // before waiting for on_cache_initialized_ signal so that
     // InitializeCacheOnIOThreadPool won't deadlock waiting to lock to update
     // data members and signal on_cache_initialized_.
     base::AutoLock lock(lock_);
     need_to_wait = cache_initialization_started_;
  }

  if (need_to_wait)
    on_cache_initialized_->Wait();

  // Lock to let root destroy cache map and resource map.
  base::AutoLock lock(lock_);
  root_.reset(NULL);
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
  // This routine will start two simultaneous feed content retrieval attempts
  // when we are dealing with the root directory - from the feed cache (local
  // disk) and from the gdata server (HTTP request).
  // The first of these two feed operations that finishes (either disk cache or
  // server retrieval) will be responsible for replying to this particular
  // search request. If the sever side content is retrieved after the cache,
  // we will also raise directory content change notification. In the opposite
  // case, the local content from the disk cache will be ignored since we
  // the server feed holds the most current state.
  // |params.delegate| implementation will ensure that the outcome of the first
  // completed operation is the one that gets reported to the caller (callback).

  // If we are refreshing the content of the root directory, try fetch the
  // feed from the disk first.
  if (params.directory_path == FilePath(kGDataRootDirectory))
      LoadRootFeed(params);

  // ...then also kick off document feed fetching from the server as well.
  // |feed_list| will contain the list of all collected feed updates that
  // we will receive through calls of DocumentsService::GetDocuments().
  scoped_ptr<base::ListValue> feed_list(new base::ListValue());
  ContinueDirectoryRefresh(params, feed_list.Pass());
}

void GDataFileSystem::Copy(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FilePath dest_parent_path = dest_file_path.DirName();

  base::AutoLock lock(lock_);
  GDataFileBase* src_file = GetGDataFileInfoFromPath(src_file_path);
  GDataFileBase* dest_parent = GetGDataFileInfoFromPath(dest_parent_path);
  if (!src_file || !dest_parent) {
    error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  } else {
    // TODO(benchan): Implement copy for regular files and directories.
    // To copy a regular file, we need to first download the file and
    // then upload it, which is not yet implemented. Also, in the interim,
    // we handle recursive directory copy in the file manager.
    if (!src_file->AsGDataFile() ||
        !src_file->AsGDataFile()->is_hosted_document()) {
      error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
    } else if (!dest_parent->AsGDataDirectory()) {
      error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    }
  }

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, error));

    return;
  }

  FilePathUpdateCallback add_file_to_directory_callback =
      base::Bind(&GDataFileSystem::AddFileToDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_parent_path,
                 callback);

  documents_service_->CopyDocument(
      src_file->self_url(),
      // Drop the document extension, which should not be in the document title.
      dest_file_path.BaseName().RemoveExtension().value(),
      base::Bind(&GDataFileSystem::OnCopyDocumentCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 add_file_to_directory_callback));
}

void GDataFileSystem::Rename(const FilePath& file_path,
                             const FilePath::StringType& new_name,
                             const FilePathUpdateCallback& callback) {
  // It is a no-op if the file is renamed to the same name.
  if (file_path.BaseName().value() == new_name) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, base::PLATFORM_FILE_OK, file_path));
    }
    return;
  }

  base::AutoLock lock(lock_);
  GDataFileBase* file = GetGDataFileInfoFromPath(file_path);
  if (!file) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND, file_path));
    }
    return;
  }

  // Drop the .g<something> extension from |new_name| if the file being
  // renamed is a hosted document and |new_name| has the same .g<something>
  // extension as the file.
  FilePath::StringType file_name = new_name;
  if (file->AsGDataFile() && file->AsGDataFile()->is_hosted_document()) {
    FilePath new_file(file_name);
    if (new_file.Extension() == file->AsGDataFile()->document_extension()) {
      file_name = new_file.RemoveExtension().value();
    }
  }

  documents_service_->RenameResource(
      file->self_url(),
      file_name,
      base::Bind(&GDataFileSystem::OnRenameResourceCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 file_name,
                 callback));
}

void GDataFileSystem::Move(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FilePath dest_parent_path = dest_file_path.DirName();

  {
    // This scoped lock needs to be released before calling Rename() below.
    base::AutoLock lock(lock_);
    GDataFileBase* src_file = GetGDataFileInfoFromPath(src_file_path);
    GDataFileBase* dest_parent = GetGDataFileInfoFromPath(dest_parent_path);
    if (!src_file || !dest_parent) {
      error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    } else {
      if (!dest_parent->AsGDataDirectory())
        error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    }

    if (error != base::PLATFORM_FILE_OK) {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback, error));
      }
      return;
    }
  }

  // If the file/directory is moved to the same directory, just rename it.
  if (src_file_path.DirName() == dest_parent_path) {
    FilePathUpdateCallback final_file_path_update_callback =
        base::Bind(&GDataFileSystem::OnFilePathUpdated,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback);

    Rename(src_file_path, dest_file_path.BaseName().value(),
           final_file_path_update_callback);
    return;
  }

  // Otherwise, the move operation involves three steps:
  // 1. Renames the file at |src_file_path| to basename(|dest_file_path|)
  //    within the same directory. The rename operation is a no-op if
  //    basename(|src_file_path|) equals to basename(|dest_file_path|).
  // 2. Removes the file from its parent directory (the file is not deleted),
  //    which effectively moves the file to the root directory.
  // 3. Adds the file to the parent directory of |dest_file_path|, which
  //    effectively moves the file from the root directory to the parent
  //    directory of |dest_file_path|.
  FilePathUpdateCallback add_file_to_directory_callback =
      base::Bind(&GDataFileSystem::AddFileToDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path.DirName(),
                 callback);

  FilePathUpdateCallback remove_file_from_directory_callback =
      base::Bind(&GDataFileSystem::RemoveFileFromDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 src_file_path.DirName(),
                 add_file_to_directory_callback);

  Rename(src_file_path, dest_file_path.BaseName().value(),
         remove_file_from_directory_callback);
}

void GDataFileSystem::AddFileToDirectory(const FilePath& dir_path,
                                         const FileOperationCallback& callback,
                                         base::PlatformFileError error,
                                         const FilePath& file_path) {
  base::AutoLock lock(lock_);
  GDataFileBase* file = GetGDataFileInfoFromPath(file_path);
  GDataFileBase* dir = GetGDataFileInfoFromPath(dir_path);
  if (error == base::PLATFORM_FILE_OK) {
    if (!file || !dir) {
      error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    } else {
      if (!dir->AsGDataDirectory())
        error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    }
  }

  // Returns if there is an error or |dir_path| is the root directory.
  if (error != base::PLATFORM_FILE_OK || dir->AsGDataRootDirectory()) {
    if (!callback.is_null())
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, error));

    return;
  }

  documents_service_->AddResourceToDirectory(
      dir->content_url(),
      file->self_url(),
      base::Bind(&GDataFileSystem::OnAddFileToDirectoryCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 file_path,
                 dir_path));
}

void GDataFileSystem::RemoveFileFromDirectory(
    const FilePath& dir_path,
    const FilePathUpdateCallback& callback,
    base::PlatformFileError error,
    const FilePath& file_path) {
  base::AutoLock lock(lock_);
  GDataFileBase* file = GetGDataFileInfoFromPath(file_path);
  GDataFileBase* dir = GetGDataFileInfoFromPath(dir_path);
  if (error == base::PLATFORM_FILE_OK) {
    if (!file || !dir) {
      error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    } else {
      if (!dir->AsGDataDirectory())
        error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    }
  }

  // Returns if there is an error or |dir_path| is the root directory.
  if (error != base::PLATFORM_FILE_OK || dir->AsGDataRootDirectory()) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, error, file_path));
    }
    return;
  }

  documents_service_->RemoveResourceFromDirectory(
      dir->content_url(),
      file->self_url(),
      file->resource_id(),
      base::Bind(&GDataFileSystem::OnRemoveFileFromDirectoryCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 file_path,
                 dir_path));
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
      file_info->GetFilePath(),
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
    const FilePath& virtual_path,
    const InitiateUploadCallback& callback) {
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
                                 destination_directory_url,
                                 virtual_path),
          base::Bind(&GDataFileSystem::OnUploadLocationReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     // MessageLoopProxy is used to run |callback| on the
                     // thread where this function was called.
                     base::MessageLoopProxy::current()));
}

void GDataFileSystem::OnUploadLocationReceived(
    const InitiateUploadCallback& callback,
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
    const ResumeUploadCallback& callback) {
  documents_service_->ResumeUpload(
          params,
          base::Bind(&GDataFileSystem::OnResumeUpload,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     // MessageLoopProxy is used to run |callback| on the
                     // thread where this function was called.
                     base::MessageLoopProxy::current()));
}

void GDataFileSystem::OnResumeUpload(const ResumeUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    const ResumeUploadResponse& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null())
    message_loop_proxy->PostTask(FROM_HERE, base::Bind(callback, response));

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

bool GDataFileSystem::GetFileInfoFromPath(
    const FilePath& file_path, base::PlatformFileInfo* file_info) {
  base::AutoLock lock(lock_);
  GDataFileBase* file = GetGDataFileInfoFromPath(file_path);
  if (!file)
    return false;

  *file_info = file->file_info();
  return true;
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

void GDataFileSystem::GetFromCacheForPath(
    const FilePath& gdata_file_path,
    const GetFromCacheCallback& callback) {
  std::string resource_id;
  std::string md5;

  {  // Lock to use GetGDataFileInfoFromPath and returned pointer, but need to
     // release before GetFromCache.
    base::AutoLock lock(lock_);
    GDataFileBase* file_base = GetGDataFileInfoFromPath(gdata_file_path);

    if (file_base && file_base->AsGDataFile()) {
      GDataFile* file = file_base->AsGDataFile();
      resource_id = file->resource_id();
      md5 = file->file_md5();
    } else {
      // Invoke |callback| with error.
      if (!callback.is_null()) {
          base::MessageLoopProxy::current()->PostTask(
              FROM_HERE,
              base::Bind(callback,
                         base::PLATFORM_FILE_ERROR_NOT_FOUND,
                         std::string(),
                         std::string(),
                         gdata_file_path,
                         FilePath()));
      }
      return;
    }
  }

  GetFromCacheInternal(resource_id, md5, gdata_file_path, callback);
}

void GDataFileSystem::GetCacheState(const std::string& resource_id,
                                    const std::string& md5,
                                    const GetCacheStateCallback& callback) {
  InitializeCacheIfNecessary();

  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(GetCacheStateOnIOThreadPool,
                 resource_id,
                 md5,
                 callback,
                 base::Bind(&GDataFileSystem::OnGetCacheState,
                            weak_ptr_factory_.GetWeakPtr()),
                 base::MessageLoopProxy::current()));
}

void GDataFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  documents_service_->GetAccountMetadata(
      base::Bind(&GDataFileSystem::OnGetAvailableSpace,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void GDataFileSystem::OnGetAvailableSpace(
    const GetAvailableSpaceCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {

  base::PlatformFileError error = GDataToPlatformError(status);
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, -1, -1);
    return;
  }

  scoped_ptr<AccountMetadataFeed> feed;
  if (data.get())
      feed.reset(AccountMetadataFeed::CreateFrom(data.get()));
  if (!feed.get()) {
    callback.Run(base::PLATFORM_FILE_ERROR_FAILED, -1, -1);
    return;
  }

  SaveFeed(data.Pass(), FilePath(kAccountMetadataFile));

  callback.Run(base::PLATFORM_FILE_OK,
               feed->quota_bytes_total(),
               feed->quota_bytes_used());
}

std::vector<GDataOperationRegistry::ProgressStatus>
    GDataFileSystem::GetProgressStatusList() {
  return documents_service_->operation_registry()->GetProgressStatusList();
}

void GDataFileSystem::AddOperationObserver(
    GDataOperationRegistry::Observer* observer) {
  return documents_service_->operation_registry()->AddObserver(observer);
}

void GDataFileSystem::RemoveOperationObserver(
    GDataOperationRegistry::Observer* observer) {
  return documents_service_->operation_registry()->RemoveObserver(observer);
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
                                          feed_list.get(),
                                          FROM_SERVER);
  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  // If this was the root feed, cache its content.
  if (params.directory_path == FilePath(kGDataRootDirectory)) {
    scoped_ptr<base::Value> feed_list_value(feed_list.release());
    SaveFeed(feed_list_value.Pass(), FilePath(kLastFeedFile));
  }

  // Continue file content search operation if the delegate hasn't terminated
  // this search branch already.
  if (!params.delegate->had_terminated())
    FindFileByPath(params.file_path, params.delegate);
}

void GDataFileSystem::LoadRootFeed(
    const GDataFileSystem::FindFileParams& params) {

  BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&GDataFileSystem::LoadRootFeedOnIOThreadPool,
                 cache_paths_[CACHE_TYPE_META].Append(kLastFeedFile),
                 base::MessageLoopProxy::current(),
                 base::Bind(&GDataFileSystem::OnLoadRootFeed,
                            weak_ptr_factory_.GetWeakPtr(),
                            params)));
}

void GDataFileSystem::OnLoadRootFeed(
    const GDataFileSystem::FindFileParams& params,
    base::PlatformFileError error,
    scoped_ptr<base::Value> feed_list) {

  if (error == base::PLATFORM_FILE_OK &&
      (!feed_list.get() || feed_list->GetType() != Value::TYPE_LIST)) {
    LOG(WARNING) << "No feed content!";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (error != base::PLATFORM_FILE_OK) {
    // Report error only if the other branch had already completed, otherwise
    // the hope is that feed retrieval over HTTP might still succeed.
    if (params.delegate->had_terminated())
      params.delegate->OnError(error);
    return;
  }

  // Don't update root directory content if we have already terminated this
  // delegate.
  if (params.delegate->had_terminated())
    return;

  error = UpdateDirectoryWithDocumentFeed(
      params.directory_path,
      reinterpret_cast<base::ListValue*>(feed_list.get()),
      FROM_CACHE);
  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  // Continue file content search operation if the delegate hasn't terminated
  // this search branch already.
  FindFileByPath(params.file_path, params.delegate);
}

// Static.
void GDataFileSystem::LoadRootFeedOnIOThreadPool(
    const FilePath& file_path,
    scoped_refptr<base::MessageLoopProxy> relay_proxy ,
    const GetJsonDocumentCallback& callback) {

  scoped_ptr<base::Value> root_value;
  std::string contents;
  if (!file_util::ReadFileToString(file_path, &contents)) {
    relay_proxy ->PostTask(FROM_HERE,
                           base::Bind(callback,
                                      base::PLATFORM_FILE_ERROR_NOT_FOUND,
                                      base::Passed(&root_value)));
    return;
  }

  int unused_error_code = -1;
  std::string unused_error_message;
  root_value.reset(base::JSONReader::ReadAndReturnError(
      contents, false, &unused_error_code, &unused_error_message));

  bool has_root = root_value.get();
  if (!has_root)
    LOG(WARNING) << "Cached content read failed for file " << file_path.value();

  relay_proxy ->PostTask(FROM_HERE,
      base::Bind(callback,
                 has_root ? base::PLATFORM_FILE_OK :
                            base::PLATFORM_FILE_ERROR_FAILED,
                 base::Passed(&root_value)));
}


void GDataFileSystem::OnFilePathUpdated(const FileOperationCallback& callback,
                                        base::PlatformFileError error,
                                        const FilePath& file_path) {
  if (!callback.is_null())
    callback.Run(error);
}

void GDataFileSystem::OnRenameResourceCompleted(
    const FilePath& file_path,
    const FilePath::StringType& new_name,
    const FilePathUpdateCallback& callback,
    GDataErrorCode status,
    const GURL& document_url) {
  FilePath updated_file_path;
  base::PlatformFileError error = GDataToPlatformError(status);
  if (error == base::PLATFORM_FILE_OK)
    error = RenameFileOnFilesystem(file_path, new_name, &updated_file_path);

  if (!callback.is_null())
    callback.Run(error, updated_file_path);
}

void GDataFileSystem::OnCopyDocumentCompleted(
    const FilePathUpdateCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  base::PlatformFileError error = GDataToPlatformError(status);
  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, FilePath());

    return;
  }

  base::DictionaryValue* dict_value = NULL;
  base::Value* entry_value = NULL;
  if (data.get() && data->GetAsDictionary(&dict_value) && dict_value)
    dict_value->Get("entry", &entry_value);

  if (!entry_value) {
    if (!callback.is_null())
      callback.Run(base::PLATFORM_FILE_ERROR_FAILED, FilePath());

    return;
  }

  scoped_ptr<DocumentEntry> entry(DocumentEntry::CreateFrom(entry_value));
  if (!entry.get()) {
    if (!callback.is_null())
      callback.Run(base::PLATFORM_FILE_ERROR_FAILED, FilePath());

    return;
  }

  FilePath file_path;
  {
    base::AutoLock lock(lock_);
    GDataFileBase* file =
        GDataFileBase::FromDocumentEntry(root_.get(), entry.get(), root_.get());
    if (!file) {
      if (!callback.is_null())
        callback.Run(base::PLATFORM_FILE_ERROR_FAILED, FilePath());

      return;
    }
    root_->AddFile(file);
    file_path = file->GetFilePath();
  }

  if (!callback.is_null())
    callback.Run(error, file_path);
}

void GDataFileSystem::OnAddFileToDirectoryCompleted(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    const FilePath& dir_path,
    GDataErrorCode status,
    const GURL& document_url) {
  base::PlatformFileError error = GDataToPlatformError(status);
  if (error == base::PLATFORM_FILE_OK)
    error = AddFileToDirectoryOnFilesystem(file_path, dir_path);

  if (!callback.is_null())
    callback.Run(error);
}

void GDataFileSystem::OnRemoveFileFromDirectoryCompleted(
    const FilePathUpdateCallback& callback,
    const FilePath& file_path,
    const FilePath& dir_path,
    GDataErrorCode status,
    const GURL& document_url) {
  FilePath updated_file_path = file_path;
  base::PlatformFileError error = GDataToPlatformError(status);
  if (error == base::PLATFORM_FILE_OK)
    error = RemoveFileFromDirectoryOnFilesystem(file_path, dir_path,
                                                &updated_file_path);

  if (!callback.is_null())
    callback.Run(error, updated_file_path);
}

void GDataFileSystem::SaveFeed(scoped_ptr<base::Value> feed,
                               const FilePath& name) {
  BrowserThread::PostBlockingPoolSequencedTask(kGDataFileSystemToken, FROM_HERE,
      base::Bind(&GDataFileSystem::SaveFeedOnIOThreadPool,
                 cache_paths_[CACHE_TYPE_META],
                 base::Passed(&feed),
                 name));
}

// Static.
void GDataFileSystem::SaveFeedOnIOThreadPool(
    const FilePath& meta_cache_path,
    scoped_ptr<base::Value> feed,
    const FilePath& name) {
  if (!file_util::DirectoryExists(meta_cache_path)) {
    if (!file_util::CreateDirectory(meta_cache_path)) {
      LOG(WARNING) << "GData metadata cache directory can't be created at "
                   << meta_cache_path.value();
      return;
    }
  }

  FilePath file_name = meta_cache_path.Append(name);
  std::string json;
  base::JSONWriter::Write(feed.get(), &json);

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

base::PlatformFileError GDataFileSystem::RenameFileOnFilesystem(
    const FilePath& file_path,
    const FilePath::StringType& new_name,
    FilePath* updated_file_path) {
  DCHECK(updated_file_path);

  base::AutoLock lock(lock_);

  scoped_refptr<ReadOnlyFindFileDelegate> find_file_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, find_file_delegate);

  GDataFileBase* file = find_file_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  DCHECK(file->parent());
  file->set_title(new_name);
  // After changing the title of the file, call TakeFile() to remove the
  // file from its parent directory and then add it back in order to go
  // through the file name de-duplication.
  if (!file->parent()->TakeFile(file))
    return base::PLATFORM_FILE_ERROR_FAILED;

  *updated_file_path = file->GetFilePath();
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::AddFileToDirectoryOnFilesystem(
    const FilePath& file_path, const FilePath& dir_path) {
  base::AutoLock lock(lock_);

  scoped_refptr<ReadOnlyFindFileDelegate> find_file_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, find_file_delegate);

  GDataFileBase* file = find_file_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  DCHECK_EQ(root_.get(), file->parent());

  scoped_refptr<ReadOnlyFindFileDelegate> find_dir_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(dir_path, find_dir_delegate);
  GDataFileBase* dir = find_dir_delegate->file();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  if (!dir->AsGDataDirectory())
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  if (!dir->AsGDataDirectory()->TakeFile(file))
    return base::PLATFORM_FILE_ERROR_FAILED;

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::RemoveFileFromDirectoryOnFilesystem(
    const FilePath& file_path, const FilePath& dir_path,
    FilePath* updated_file_path) {
  DCHECK(updated_file_path);

  base::AutoLock lock(lock_);

  scoped_refptr<ReadOnlyFindFileDelegate> find_file_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, find_file_delegate);

  GDataFileBase* file = find_file_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  scoped_refptr<ReadOnlyFindFileDelegate> find_dir_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(dir_path, find_dir_delegate);
  GDataFileBase* dir = find_dir_delegate->file();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  if (!dir->AsGDataDirectory())
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  DCHECK_EQ(dir->AsGDataDirectory(), file->parent());

  if (!root_->TakeFile(file))
    return base::PLATFORM_FILE_ERROR_FAILED;

  *updated_file_path = file->GetFilePath();
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::RemoveFileFromFileSystem(
    const FilePath& file_path) {

  std::string resource_id;
  base::PlatformFileError error = RemoveFileFromGData(file_path, &resource_id);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  // If resource_id is not empty, remove its corresponding file from cache.
  if (!resource_id.empty()) {
    RemoveFromCache(resource_id,
                    base::Bind(&GDataFileSystem::OnRemovedFromCache,
                               weak_ptr_factory_.GetWeakPtr()));
  }

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

base::PlatformFileError GDataFileSystem::UpdateRootWithDocumentFeed(
    base::ListValue* feed_list) {
// A map of self URLs to pairs of gdata file and parent URL.
  typedef std::map<GURL, std::pair<GDataFileBase*, GURL> >
      UrlToFileAndParentMap;
  UrlToFileAndParentMap file_by_url;
  bool first_feed = true;

  for (base::ListValue::const_iterator feeds_iter = feed_list->begin();
       feeds_iter != feed_list->end(); ++feeds_iter) {
    scoped_ptr<DocumentFeed> feed(ParseDocumentFeed(*feeds_iter));
    if (!feed.get())
      return base::PLATFORM_FILE_ERROR_FAILED;

    // Get upload url from the root feed. Links for all other collections will
    // be handled in GDatadirectory::FromDocumentEntry();
    if (first_feed) {
      const Link* root_feed_upload_link =
          feed->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
      if (root_feed_upload_link)
        root_->set_upload_url(root_feed_upload_link->href());
      first_feed = false;
    }

    for (ScopedVector<DocumentEntry>::const_iterator iter =
             feed->entries().begin();
         iter != feed->entries().end(); ++iter) {
      DocumentEntry* doc = *iter;
      GDataFileBase* file = GDataFileBase::FromDocumentEntry(NULL, doc,
                                                             root_.get());
      // Some document entries don't map into files (i.e. sites).
      if (!file)
        continue;

      GURL parent_url;
      const Link* parent_link = doc->GetLinkByType(Link::PARENT);
      if (parent_link)
        parent_url = parent_link->href();
      file_by_url[file->self_url()] = std::make_pair(file, parent_url);
    }
  }

  for (UrlToFileAndParentMap::iterator it = file_by_url.begin();
       it != file_by_url.end(); ++it) {
    GDataFileBase* file = it->second.first;
    GURL parent_url = it->second.second;
    GDataDirectory* dir = root_.get();
    if (!parent_url.is_empty()) {
      DCHECK(file_by_url.find(parent_url) != file_by_url.end());
      dir = file_by_url[parent_url].first->AsGDataDirectory();
    }
    DCHECK(dir);

    dir->AddFile(file);
  }

  NotifyDirectoryChanged(root_->GetFilePath());

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::UpdateDirectoryWithDocumentFeed(
    const FilePath& directory_path,
    base::ListValue* feed_list,
    ContentOrigin origin) {
  DVLOG(1) << "Updating directory content of  "
           << directory_path.value().c_str()
           << " with feed from "
           << (origin == FROM_CACHE ? "cache" : "web server");

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

  dir->set_origin(origin);
  dir->set_refresh_time(base::Time::Now());

  // Remove all child elements if we are refreshing the entire content.
  dir->RemoveChildren();

  if (dir == root_.get())
    return UpdateRootWithDocumentFeed(feed_list);

  // Get through all collected feeds and fill in directory structure for all
  // of them.
  for (base::ListValue::const_iterator feeds_iter = feed_list->begin();
       feeds_iter != feed_list->end(); ++feeds_iter) {
    scoped_ptr<DocumentFeed> feed(ParseDocumentFeed(*feeds_iter));
    if (!feed.get())
      return base::PLATFORM_FILE_ERROR_FAILED;

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

      GDataFileBase* file = GDataFileBase::FromDocumentEntry(dir, doc,
                                                             root_.get());
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
                                                             entry.get(),
                                                             root_.get());
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

base::PlatformFileError GDataFileSystem::RemoveFileFromGData(
    const FilePath& file_path, std::string* resource_id) {
  resource_id->clear();

  // We need to lock here as well (despite FindFileByPath lock) since
  // directory instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, update_delegate);

  GDataFileBase* file = update_delegate->file();

  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // You can't remove root element.
  if (!file->parent())
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

  // If it's a file (only files have resource id), get its resource id so that
  // we can remove it after releasing the auto lock.
  if (file->AsGDataFile())
    *resource_id = file->AsGDataFile()->resource_id();

  if (!file->parent()->RemoveFile(file))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  return base::PLATFORM_FILE_OK;
}

//===================== GDataFileSystem: Cache entry points ====================

FilePath GDataFileSystem::GetCacheFilePath(const std::string& resource_id,
                                           const std::string& md5) {
  // Runs on any thread.
  // Filename is formatted as resource_id.md5, i.e. resource_id is the base
  // name and md5 is the extension.
  FilePath path(GDataFileBase::EscapeUtf8FileName(resource_id) +
                FilePath::kExtensionSeparator +
                GDataFileBase::EscapeUtf8FileName(md5));
  return cache_paths_[CACHE_TYPE_BLOBS].Append(path);
}

void GDataFileSystem::InitializeCacheIfNecessary() {
  // Lock to access cache_initialized_started_;
  base::AutoLock lock(lock_);
  if (cache_initialization_started_) {
    // Cache initialization is either in progress or has completed.
    return;
  }

  // Need to initialize cache.

  cache_initialization_started_ = true;

  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(&GDataFileSystem::InitializeCacheOnIOThreadPool,
                 base::Unretained(this)));
}

void GDataFileSystem::StoreToCache(const std::string& resource_id,
                                   const std::string& md5,
                                   const FilePath& source_path,
                                   const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(StoreToCacheOnIOThreadPool,
                 StoreToCacheParams(
                     resource_id,
                     md5,
                     source_path,
                     callback,
                     GetCacheFilePath(resource_id, md5),
                     base::Bind(&GDataFileSystem::OnStoredToCache,
                                weak_ptr_factory_.GetWeakPtr()),
                     base::MessageLoopProxy::current())));
}

void GDataFileSystem::GetFromCache(const std::string& resource_id,
                                   const std::string& md5,
                                   const GetFromCacheCallback& callback) {
  GetFromCacheInternal(resource_id, md5, FilePath(), callback);
}

void GDataFileSystem::RemoveFromCache(const std::string& resource_id,
                                      const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  // Lock to access cache map.
  base::AutoLock lock(lock_);
  root_->RemoveFromCacheMap(resource_id);

  // Post task to delete all cache versions of resource_id.
  // If $resource_id.* is passed to DeleteStaleCacheVersions, then all
  // $resource_id.* will be deleted since no file will match "$resource_id.*".
  FilePath files_to_delete = GetCacheFilePath(resource_id, kWildCard);
  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(DeleteStaleCacheVersionsWithCallback,
                 resource_id,
                 callback,
                 files_to_delete,
                 base::MessageLoopProxy::current()));

}

void GDataFileSystem::Pin(const std::string& resource_id,
                          const std::string& md5,
                          const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(ModifyCacheStatusOnIOThreadPool,
                 ModifyCacheStatusParams(
                     resource_id,
                     md5,
                     callback,
                     GDataRootDirectory::CACHE_PINNED,
                     true,
                     GetCacheFilePath(resource_id, md5),
                     base::Bind(&GDataFileSystem::OnCacheStatusModified,
                                weak_ptr_factory_.GetWeakPtr()),
                     base::MessageLoopProxy::current())));
}

void GDataFileSystem::Unpin(const std::string& resource_id,
                            const std::string& md5,
                            const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(ModifyCacheStatusOnIOThreadPool,
                 ModifyCacheStatusParams(
                     resource_id,
                     md5,
                     callback,
                     GDataRootDirectory::CACHE_PINNED,
                     false,
                     GetCacheFilePath(resource_id, md5),
                     base::Bind(&GDataFileSystem::OnCacheStatusModified,
                                weak_ptr_factory_.GetWeakPtr()),
                     base::MessageLoopProxy::current())));
}

//========= GDataFileSystem: Cache tasks that ran on io thread pool ============

void GDataFileSystem::InitializeCacheOnIOThreadPool() {
  base::PlatformFileError error = CreateCacheDirectories(cache_paths_);

  if (error != base::PLATFORM_FILE_OK) {
    // Signal that cache initialization has completed.
    on_cache_initialized_->Signal();
    return;
  }

  // Scan cache directory to enumerate all files in it, retrieve their file
  // attributes and creates corresponding entries for cache map.
  GDataRootDirectory::CacheMap cache_map;
  file_util::FileEnumerator traversal(cache_paths_[CACHE_TYPE_BLOBS], false,
                                      file_util::FileEnumerator::FILES,
                                      kWildCard);
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // Extract resource_id and md5 from filename.
    FilePath base_name = current.BaseName();
    // FilePath::Extension returns ".", so strip it.
    std::string md5 = gdata::GDataFileBase::UnescapeUtf8FileName(
        base_name.Extension().substr(1));
    std::string resource_id = gdata::GDataFileBase::UnescapeUtf8FileName(
        base_name.RemoveExtension().value());

    // Retrieve mode bits of file.
    file_util::FileEnumerator::FindInfo info;
    traversal.GetFindInfo(&info);
    mode_t mode_bits = info.stat.st_mode;

    // Insert a CacheEntry for current cached file into ResourceMap.
    gdata::GDataRootDirectory::CacheEntry* entry =
        new gdata::GDataRootDirectory::CacheEntry(md5, mode_bits);
    cache_map.insert(std::make_pair(resource_id, entry));
  }

  {  // Lock to update cache map.
    base::AutoLock lock(lock_);
    root_->SetCacheMap(cache_map);
  }

  // Signal that cache initialization has completed.
  on_cache_initialized_->Signal();
}

//=== GDataFileSystem: Cache callbacks for tasks that ran on io thread pool ====

void GDataFileSystem::OnStoredToCache(base::PlatformFileError error,
                                      const std::string& resource_id,
                                      const std::string& md5,
                                      mode_t mode_bits,
                                      const CacheOperationCallback& callback) {
  DVLOG(1) << "OnStoredToCache: " << error;

  if (error == base::PLATFORM_FILE_OK) {
    // Lock to update cache map.
    base::AutoLock lock(lock_);
    root_->UpdateCacheMap(resource_id, md5, mode_bits);
  }

  // Invoke callback.
  if (!callback.is_null())
    callback.Run(error, resource_id, md5);
}

void GDataFileSystem::OnGetFromCache(const std::string& resource_id,
                                     const std::string& md5,
                                     const FilePath& gdata_file_path,
                                     const GetFromCacheCallback& callback) {
  DVLOG(1) << "OnGetFromCache: ";

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FilePath path;

  // Lock to access cache map.
  base::AutoLock lock(lock_);
  if (root_->CacheFileExists(resource_id, md5)) {
    path = GetCacheFilePath(resource_id, md5);
  } else {
    error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }

  // Invoke callback.
  if (!callback.is_null())
    callback.Run(error, resource_id, md5, gdata_file_path, path);
}

void GDataFileSystem::OnRemovedFromCache(base::PlatformFileError error,
                                         const std::string& resource_id,
                                         const std::string& md5) {
  DVLOG(1) << "OnRemovedFromCache: " << error;
}

void GDataFileSystem::OnCacheStatusModified(
    base::PlatformFileError error,
    const std::string& resource_id,
    const std::string& md5,
    mode_t mode_bits,
    const CacheOperationCallback& callback) {
  DVLOG(1) << "OnCacheStatusModified: " << error;

  if (error == base::PLATFORM_FILE_OK) {
    // Lock to update cache map.
    base::AutoLock lock(lock_);
    root_->UpdateCacheMap(resource_id, md5, mode_bits);
  }

  // Invoke callback.
  if (!callback.is_null())
    callback.Run(error, resource_id, md5);
}

void GDataFileSystem::OnGetCacheState(const std::string& resource_id,
                                      const std::string& md5,
                                      const GetCacheStateCallback& callback) {
  DVLOG(1) << "OnGetCacheState: ";

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  int cache_state = GDataFile::CACHE_STATE_NONE;

  // Lock to access resource and cache maps.
  base::AutoLock lock(lock_);
  // Get file object for |resource_id|.
  GDataFileBase* file_base = root_->GetFileByResource(resource_id);
  GDataFile* file = NULL;
  if (!file_base || !file_base->AsGDataFile()) {
    error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  } else {
    file = file_base->AsGDataFile();
    // Get cache state of file corresponding to |resource_id| and |md5|.
    cache_state = root_->GetCacheState(resource_id, md5);
  }

  // Invoke callback.
  if (!callback.is_null())
    callback.Run(error, file, cache_state);
}

//============= GDataFileSystem: internal helper functions =====================

void GDataFileSystem::GetFromCacheInternal(
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& gdata_file_path,
    const GetFromCacheCallback& callback) {
  InitializeCacheIfNecessary();

  BrowserThread::PostBlockingPoolSequencedTask(
      kGDataFileSystemToken,
      FROM_HERE,
      base::Bind(GetFromCacheOnIOThreadPool,
                 resource_id,
                 md5,
                 gdata_file_path,
                 callback,
                 base::Bind(&GDataFileSystem::OnGetFromCache,
                            weak_ptr_factory_.GetWeakPtr()),
                 base::MessageLoopProxy::current()));
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
