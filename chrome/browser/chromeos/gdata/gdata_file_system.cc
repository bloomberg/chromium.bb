// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_protocol_handler.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/base/mime_util.h"

using content::BrowserThread;

namespace gdata {
namespace {

const char kMimeTypeJson[] = "application/json";
const char kMimeTypeOctetStream[] = "application/octet-stream";

const char kEmptyFilePath[] = "/dev/null";

// GData update check interval (in seconds).
#ifndef NDEBUG
const int kGDataUpdateCheckIntervalInSec = 5;
#else
const int kGDataUpdateCheckIntervalInSec = 60;
#endif

//================================ Helper functions ============================

// Invoked upon completion of TransferRegularFile initiated by Copy.
//
// |callback| is run on the thread represented by |relay_proxy|.
void OnTransferRegularFileCompleteForCopy(
    const FileOperationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataFileError error) {
  if (!callback.is_null())
    relay_proxy->PostTask(FROM_HERE, base::Bind(callback, error));
}

// Runs GetFileCallback with pointers dereferenced.
// Used for PostTaskAndReply().
void RunGetFileCallbackHelper(const GetFileCallback& callback,
                              GDataFileError* error,
                              FilePath* file_path,
                              std::string* mime_type,
                              GDataFileType* file_type) {
  DCHECK(error);
  DCHECK(file_path);
  DCHECK(mime_type);
  DCHECK(file_type);

  if (!callback.is_null())
    callback.Run(*error, *file_path, *mime_type, *file_type);
}

// Ditto for FileOperationCallback
void RunFileOperationCallbackHelper(
    const FileOperationCallback& callback,
    GDataFileError* error) {
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error);
}

// Callback for cache file operations invoked by AddUploadedFileOnUIThread.
void OnCacheUpdatedForAddUploadedFile(
    const base::Closure& callback,
    GDataFileError /* error */,
    const std::string& /* resource_id */,
    const std::string& /* md5 */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null())
    callback.Run();
}

// Helper function called upon completion of AddUploadFile invoked by
// OnTransferCompleted.
void OnAddUploadFileCompleted(
    const FileOperationCallback& callback,
    GDataFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null())
    callback.Run(error);
}

// The class to wait for the initial load of root feed and runs the callback
// after the initialization.
class InitialLoadObserver : public GDataFileSystemInterface::Observer {
 public:
  InitialLoadObserver(GDataFileSystemInterface* file_system,
                      const base::Closure& callback)
      : file_system_(file_system), callback_(callback) {}

  virtual void OnInitialLoadFinished() OVERRIDE {
    if (!callback_.is_null())
      base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback_);
    file_system_->RemoveObserver(this);
    base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  GDataFileSystemInterface* file_system_;
  base::Closure callback_;
};

// Gets the file size of |local_file|.
void GetLocalFileSizeOnBlockingPool(const FilePath& local_file,
                                    GDataFileError* error,
                                    int64* file_size) {
  DCHECK(error);
  DCHECK(file_size);

  *file_size = 0;
  *error = file_util::GetFileSize(local_file, file_size) ?
      GDATA_FILE_OK :
      GDATA_FILE_ERROR_NOT_FOUND;
}

// Gets the file size and the content type of |local_file|.
void GetLocalFileInfoOnBlockingPool(
    const FilePath& local_file,
    GDataFileError* error,
    int64* file_size,
    std::string* content_type) {
  DCHECK(error);
  DCHECK(file_size);
  DCHECK(content_type);

  if (!net::GetMimeTypeFromExtension(local_file.Extension(), content_type))
    *content_type = kMimeTypeOctetStream;

  *file_size = 0;
  *error = file_util::GetFileSize(local_file, file_size) ?
      GDATA_FILE_OK :
      GDATA_FILE_ERROR_NOT_FOUND;
}

// Checks if a local file at |local_file_path| is a JSON file referencing a
// hosted document on blocking pool, and if so, gets the resource ID of the
// document.
void GetDocumentResourceIdOnBlockingPool(
    const FilePath& local_file_path,
    std::string* resource_id) {
  DCHECK(resource_id);

  if (DocumentEntry::HasHostedDocumentExtension(local_file_path)) {
    std::string error;
    DictionaryValue* dict_value = NULL;
    JSONFileValueSerializer serializer(local_file_path);
    scoped_ptr<Value> value(serializer.Deserialize(NULL, &error));
    if (value.get() && value->GetAsDictionary(&dict_value))
      dict_value->GetString("resource_id", resource_id);
  }
}

// Creates a temporary JSON file representing a document with |edit_url|
// and |resource_id| under |document_dir| on blocking pool.
void CreateDocumentJsonFileOnBlockingPool(
    const FilePath& document_dir,
    const GURL& edit_url,
    const std::string& resource_id,
    GDataFileError* error,
    FilePath* temp_file_path,
    std::string* mime_type,
    GDataFileType* file_type) {
  DCHECK(error);
  DCHECK(temp_file_path);
  DCHECK(mime_type);
  DCHECK(file_type);

  *error = GDATA_FILE_ERROR_FAILED;

  if (file_util::CreateTemporaryFileInDir(document_dir, temp_file_path)) {
    std::string document_content = base::StringPrintf(
        "{\"url\": \"%s\", \"resource_id\": \"%s\"}",
        edit_url.spec().c_str(), resource_id.c_str());
    int document_size = static_cast<int>(document_content.size());
    if (file_util::WriteFile(*temp_file_path, document_content.data(),
                             document_size) == document_size) {
      *error = GDATA_FILE_OK;
    }
  }

  *mime_type = kMimeTypeJson;
  *file_type = HOSTED_DOCUMENT;
  if (*error != GDATA_FILE_OK)
      temp_file_path->clear();
}

// Gets the information of the file at local path |path|. The information is
// filled in |file_info|, and if it fails |result| will be assigned false.
void GetFileInfoOnBlockingPool(const FilePath& path,
                               base::PlatformFileInfo* file_info,
                               bool* result) {
  *result = file_util::GetFileInfo(path, file_info);
}

// Copies a file from |src_file_path| to |dest_file_path| on the local
// file system using file_util::CopyFile. |error| is set to
// GDATA_FILE_OK on success or GDATA_FILE_ERROR_FAILED
// otherwise.
void CopyLocalFileOnBlockingPool(
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    GDataFileError* error) {
  DCHECK(error);

  *error = file_util::CopyFile(src_file_path, dest_file_path) ?
      GDATA_FILE_OK : GDATA_FILE_ERROR_FAILED;
}

// Runs task on the thread to which |relay_proxy| belongs.
void RunTaskOnThread(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                     const base::Closure& task) {
  if (relay_proxy->BelongsToCurrentThread()) {
    task.Run();
  } else {
    const bool posted = relay_proxy->PostTask(FROM_HERE, task);
    DCHECK(posted);
  }
}

// Callback for GetEntryByResourceIdAsync.
// Adds |entry| to |results|. Runs |callback| with |results| when
// |run_callback| is true. When |entry| is not present in our local file system
// snapshot, it is not added to |results|. Instead, |entry_skipped_callback| is
// called.
void AddEntryToSearchResults(
    std::vector<SearchResultInfo>* results,
    const SearchCallback& callback,
    const base::Closure& entry_skipped_callback,
    GDataFileError error,
    bool run_callback,
    const GURL& next_feed,
    GDataEntry* entry) {
  // If a result is not present in our local file system snapshot, invoke
  // |entry_skipped_callback| and refreshes the snapshot with delta feed.
  // For example, this may happen if the entry has recently been added to the
  // drive (and we still haven't received its delta feed).
  if (entry) {
    const bool is_directory = entry->AsGDataDirectory() != NULL;
    results->push_back(SearchResultInfo(entry->GetFilePath(), is_directory));
  } else {
    if (!entry_skipped_callback.is_null())
      entry_skipped_callback.Run();
  }

  if (run_callback) {
    scoped_ptr<std::vector<SearchResultInfo> > result_vec(results);
    if (!callback.is_null())
      callback.Run(error, next_feed, result_vec.Pass());
  }
}

// Runs task on UI thread.
void RunTaskOnUIThread(const base::Closure& task) {
  RunTaskOnThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI), task);
}

// RelayCallback relays arguments for callback running on the given thread.
template<typename CallbackType>
struct RelayCallback;

// RelayCallback for callback with one argument.
template<typename T1>
struct RelayCallback<base::Callback<void(T1)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1)>& callback,
                  T1 arg1) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1));
  }
};

// RelayCallback for callback with two arguments.
template<typename T1, typename T2>
struct RelayCallback<base::Callback<void(T1, T2)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2)>& callback,
                  T1 arg1,
                  T2 arg2) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1, arg2));
  }
};

// RelayCallback for callback with two arguments, the last one is scoped_ptr.
template<typename T1, typename T2>
struct RelayCallback<base::Callback<void(T1, scoped_ptr<T2>)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, scoped_ptr<T2>)>& callback,
                  T1 arg1,
                  scoped_ptr<T2> arg2) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy,
                    base::Bind(callback, arg1, base::Passed(&arg2)));
  }
};

// RelayCallback for callback with three arguments.
template<typename T1, typename T2, typename T3>
struct RelayCallback<base::Callback<void(T1, T2, T3)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2, T3)>& callback,
                  T1 arg1,
                  T2 arg2,
                  T3 arg3) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1, arg2, arg3));
  }
};

// RelayCallback for callback with three arguments, the last one is scoped_ptr.
template<typename T1, typename T2, typename T3>
struct RelayCallback<base::Callback<void(T1, T2, scoped_ptr<T3>)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2, scoped_ptr<T3>)>& callback,
                  T1 arg1,
                  T2 arg2,
                  scoped_ptr<T3> arg3) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy,
                    base::Bind(callback, arg1, arg2, base::Passed(&arg3)));
  }
};

// RelayCallback for callback with four arguments.
template<typename T1, typename T2, typename T3, typename T4>
struct RelayCallback<base::Callback<void(T1, T2, T3, T4)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2, T3, T4)>& callback,
                  T1 arg1,
                  T2 arg2,
                  T3 arg3,
                  T4 arg4) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1, arg2, arg3, arg4));
  }
};

// Returns callback which runs the given |callback| on the current thread.
template<typename CallbackType>
CallbackType CreateRelayCallback(const CallbackType& callback) {
  return base::Bind(&RelayCallback<CallbackType>::Run,
                    base::MessageLoopProxy::current(),
                    callback);
}

// Helper function for binding |path| to GetEntryInfoWithFilePathCallback and
// create GetEntryInfoCallback.
void RunGetEntryInfoWithFilePathCallback(
    const GetEntryInfoWithFilePathCallback& callback,
    const FilePath& path,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  if (!callback.is_null())
    callback.Run(error, path, entry_proto.Pass());
}

}  // namespace

// GDataFileSystem::CreateDirectoryParams struct implementation.
struct GDataFileSystem::CreateDirectoryParams {
  CreateDirectoryParams(const FilePath& created_directory_path,
                        const FilePath& target_directory_path,
                        bool is_exclusive,
                        bool is_recursive,
                        const FileOperationCallback& callback);
  ~CreateDirectoryParams();

  const FilePath created_directory_path;
  const FilePath target_directory_path;
  const bool is_exclusive;
  const bool is_recursive;
  FileOperationCallback callback;
};

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

// GDataFileSystem::GetFileCompleteForOpenParams struct implementation.
struct GDataFileSystem::GetFileCompleteForOpenParams {
  GetFileCompleteForOpenParams(const std::string& resource_id,
                               const std::string& md5);
  ~GetFileCompleteForOpenParams();
  std::string resource_id;
  std::string md5;
};

GDataFileSystem::GetFileCompleteForOpenParams::GetFileCompleteForOpenParams(
    const std::string& resource_id,
    const std::string& md5)
    : resource_id(resource_id),
      md5(md5) {
}

GDataFileSystem::GetFileCompleteForOpenParams::~GetFileCompleteForOpenParams() {
}

// GDataFileSystem::GetFileFromCacheParams struct implementation.
struct GDataFileSystem::GetFileFromCacheParams {
  GetFileFromCacheParams(
      const FilePath& virtual_file_path,
      const FilePath& local_tmp_path,
      const GURL& content_url,
      const std::string& resource_id,
      const std::string& md5,
      const std::string& mime_type,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback);
  ~GetFileFromCacheParams();

  FilePath virtual_file_path;
  FilePath local_tmp_path;
  GURL content_url;
  std::string resource_id;
  std::string md5;
  std::string mime_type;
  const GetFileCallback get_file_callback;
  const GetDownloadDataCallback get_download_data_callback;
};

GDataFileSystem::GetFileFromCacheParams::GetFileFromCacheParams(
    const FilePath& virtual_file_path,
    const FilePath& local_tmp_path,
    const GURL& content_url,
    const std::string& resource_id,
    const std::string& md5,
    const std::string& mime_type,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback)
    : virtual_file_path(virtual_file_path),
      local_tmp_path(local_tmp_path),
      content_url(content_url),
      resource_id(resource_id),
      md5(md5),
      mime_type(mime_type),
      get_file_callback(get_file_callback),
      get_download_data_callback(get_download_data_callback) {
}

GDataFileSystem::GetFileFromCacheParams::~GetFileFromCacheParams() {
}

// GDataFileSystem::StartFileUploadParams implementation.
struct GDataFileSystem::StartFileUploadParams {
  StartFileUploadParams(const FilePath& in_local_file_path,
                        const FilePath& in_remote_file_path,
                        const FileOperationCallback& in_callback)
      : local_file_path(in_local_file_path),
        remote_file_path(in_remote_file_path),
        callback(in_callback) {}

  const FilePath local_file_path;
  const FilePath remote_file_path;
  const FileOperationCallback callback;
};

// GDataFileSystem class implementation.

GDataFileSystem::GDataFileSystem(
    Profile* profile,
    GDataCache* cache,
    DocumentsServiceInterface* documents_service,
    GDataUploaderInterface* uploader,
    DriveWebAppsRegistryInterface* webapps_registry,
    base::SequencedTaskRunner* blocking_task_runner)
    : profile_(profile),
      cache_(cache),
      uploader_(uploader),
      documents_service_(documents_service),
      webapps_registry_(webapps_registry),
      update_timer_(true /* retain_user_task */, true /* is_repeating */),
      hide_hosted_docs_(false),
      blocking_task_runner_(blocking_task_runner),
      ui_weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      ui_weak_ptr_(ui_weak_ptr_factory_.GetWeakPtr()) {
  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void GDataFileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_->Initialize(profile_);

  directory_service_.reset(new GDataDirectoryService);
  feed_loader_.reset(new GDataWapiFeedLoader(directory_service_.get(),
                                             documents_service_,
                                             webapps_registry_,
                                             cache_,
                                             blocking_task_runner_));
  feed_loader_->AddObserver(this);

  PrefService* pref_service = profile_->GetPrefs();
  hide_hosted_docs_ = pref_service->GetBoolean(prefs::kDisableGDataHostedFiles);

  InitializePreferenceObserver();
}

void GDataFileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ContentOrigin initial_origin = directory_service_->origin();
  if (initial_origin == FROM_SERVER) {
    directory_service_->set_origin(REFRESHING);
    feed_loader_->ReloadFromServerIfNeeded(
        initial_origin,
        directory_service_->largest_changestamp(),
        directory_service_->root()->GetFilePath(),
        base::Bind(&GDataFileSystem::OnUpdateChecked,
                   ui_weak_ptr_,
                   initial_origin));
  }
}

void GDataFileSystem::OnUpdateChecked(ContentOrigin initial_origin,
                                      GDataFileError error,
                                      GDataEntry* /* entry */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    directory_service_->set_origin(initial_origin);
  }
}

GDataFileSystem::~GDataFileSystem() {
  // This should be called from UI thread, from GDataSystemService shutdown.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  feed_loader_->RemoveObserver(this);

  // Cancel all the in-flight operations.
  // This asynchronously cancels the URL fetch operations.
  documents_service_->CancelAll();
}

void GDataFileSystem::AddObserver(
    GDataFileSystemInterface::Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void GDataFileSystem::RemoveObserver(
    GDataFileSystemInterface::Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void GDataFileSystem::StartUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!update_timer_.IsRunning());
  update_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(
                          kGDataUpdateCheckIntervalInSec),
                      base::Bind(&GDataFileSystem::CheckForUpdates,
                          ui_weak_ptr_));
}

void GDataFileSystem::StopUpdates() {
  // If unmount request comes from filesystem side, this method may be called
  // twice. First is just after unmounting on filesystem, second is after
  // unmounting on filemanager on JS. In other words, if this is called from
  // GDataSystemService::RemoveDriveMountPoint(), this will be called again from
  // FileBrowserEventRouter::HandleRemoteUpdateRequestOnUIThread().
  // We choose to stopping updates asynchronous without waiting for filemanager,
  // rather than waiting for completion of unmounting on filemanager.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (update_timer_.IsRunning())
    update_timer_.Stop();
}

void GDataFileSystem::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::GetEntryInfoByResourceIdOnUIThread,
                 ui_weak_ptr_,
                 resource_id,
                 CreateRelayCallback(callback)));
}

void GDataFileSystem::GetEntryInfoByResourceIdOnUIThread(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  directory_service_->GetEntryByResourceIdAsync(resource_id,
      base::Bind(&GDataFileSystem::GetEntryInfoByEntryOnUIThread,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::GetEntryInfoByEntryOnUIThread(
    const GetEntryInfoWithFilePathCallback& callback,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry) {
    scoped_ptr<GDataEntryProto> entry_proto(new GDataEntryProto);
    entry->ToProtoFull(entry_proto.get());
    CheckLocalModificationAndRun(
        entry_proto.Pass(),
        base::Bind(&RunGetEntryInfoWithFilePathCallback,
                   callback, entry->GetFilePath()));
  } else {
    callback.Run(GDATA_FILE_ERROR_NOT_FOUND,
                 FilePath(),
                 scoped_ptr<GDataEntryProto>());
  }
}

void GDataFileSystem::FindEntryByPathAsyncOnUIThread(
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (directory_service_->origin() == INITIALIZING) {
    // If root feed is not initialized but the initialization process has
    // already started, add an observer to execute the remaining task after
    // the end of the initialization.
    AddObserver(new InitialLoadObserver(
        this,
        base::Bind(&GDataFileSystem::FindEntryByPathSyncOnUIThread,
                   ui_weak_ptr_,
                   search_file_path,
                   callback)));
    return;
  } else if (directory_service_->origin() == UNINITIALIZED) {
    // Load root feed from this disk cache. Upon completion, kick off server
    // fetching.
    directory_service_->set_origin(INITIALIZING);
    feed_loader_->LoadFromCache(
        true,  // should_load_from_server
        search_file_path,
        // This is the initial load, hence we'll notify when it's done.
        base::Bind(&GDataFileSystem::RunAndNotifyInitialLoadFinished,
                   ui_weak_ptr_,
                   callback));
    return;
  }

  // Post a task to the same thread, rather than calling it here, as
  // FindEntryByPath() is asynchronous.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&GDataFileSystem::FindEntryByPathSyncOnUIThread,
                 ui_weak_ptr_,
                 search_file_path,
                 callback));
}

void GDataFileSystem::FindEntryByPathSyncOnUIThread(
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  directory_service_->FindEntryByPathAndRunSync(search_file_path, callback);
}

void GDataFileSystem::TransferFileFromRemoteToLocal(
    const FilePath& remote_src_file_path,
    const FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetFileByPath(remote_src_file_path,
      base::Bind(&GDataFileSystem::OnGetFileCompleteForTransferFile,
                 ui_weak_ptr_,
                 local_dest_file_path,
                 callback),
      GetDownloadDataCallback());
}

void GDataFileSystem::TransferFileFromLocalToRemote(
    const FilePath& local_src_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Make sure the destination directory exists.
  directory_service_->GetEntryInfoByPath(
      remote_dest_file_path.DirName(),
      base::Bind(
          &GDataFileSystem::TransferFileFromLocalToRemoteAfterGetEntryInfo,
          ui_weak_ptr_,
          local_src_file_path,
          remote_dest_file_path,
          callback));
}

void GDataFileSystem::TransferFileFromLocalToRemoteAfterGetEntryInfo(
    const FilePath& local_src_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != GDATA_FILE_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry_proto.get());
  if (!entry_proto->file_info().is_directory()) {
    // The parent of |remote_dest_file_path| is not a directory.
    callback.Run(GDATA_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  std::string* resource_id = new std::string;
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GetDocumentResourceIdOnBlockingPool,
                 local_src_file_path,
                 resource_id),
      base::Bind(&GDataFileSystem::TransferFileForResourceId,
                 ui_weak_ptr_,
                 local_src_file_path,
                 remote_dest_file_path,
                 callback,
                 base::Owned(resource_id)));
}

void GDataFileSystem::TransferFileForResourceId(
    const FilePath& local_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    std::string* resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(resource_id);
  DCHECK(!callback.is_null());

  if (resource_id->empty()) {
    // If |resource_id| is empty, upload the local file as a regular file.
    TransferRegularFile(local_file_path, remote_dest_file_path, callback);
    return;
  }

  // Otherwise, copy the document on the server side and add the new copy
  // to the destination directory (collection).
  CopyDocumentToDirectory(
      remote_dest_file_path.DirName(),
      *resource_id,
      // Drop the document extension, which should not be
      // in the document title.
      remote_dest_file_path.BaseName().RemoveExtension().value(),
      callback);
}

void GDataFileSystem::TransferRegularFile(
    const FilePath& local_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  int64* file_size = new int64;
  std::string* content_type = new std::string;
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GetLocalFileInfoOnBlockingPool,
                 local_file_path,
                 error,
                 file_size,
                 content_type),
      base::Bind(&GDataFileSystem::StartFileUploadOnUIThread,
                 ui_weak_ptr_,
                 StartFileUploadParams(local_file_path,
                                       remote_dest_file_path,
                                       callback),
                 base::Owned(error),
                 base::Owned(file_size),
                 base::Owned(content_type)));
}

void GDataFileSystem::StartFileUploadOnUIThread(
    const StartFileUploadParams& params,
    GDataFileError* error,
    int64* file_size,
    std::string* content_type) {
  // This method needs to run on the UI thread as required by
  // GDataUploader::UploadNewFile().
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  DCHECK(file_size);
  DCHECK(content_type);

  if (*error != GDATA_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(*error);

    return;
  }

  // Make sure the destination directory exists.
  directory_service_->GetEntryInfoByPath(
      params.remote_file_path.DirName(),
      base::Bind(
          &GDataFileSystem::StartFileUploadOnUIThreadAfterGetEntryInfo,
          ui_weak_ptr_,
          params,
          *file_size,
          *content_type));
}

void GDataFileSystem::StartFileUploadOnUIThreadAfterGetEntryInfo(
    const StartFileUploadParams& params,
    int64 file_size,
    std::string content_type,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_proto.get() && !entry_proto->file_info().is_directory())
    error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;

  if (error != GDATA_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(error);
    return;
  }
  DCHECK(entry_proto.get());

  // Fill in values of UploadFileInfo.
  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo);
  upload_file_info->file_path = params.local_file_path;
  upload_file_info->file_size = file_size;
  upload_file_info->gdata_path = params.remote_file_path;
  // Use the file name as the title.
  upload_file_info->title = params.remote_file_path.BaseName().value();
  upload_file_info->content_length = file_size;
  upload_file_info->all_bytes_present = true;
  upload_file_info->content_type = content_type;
  upload_file_info->initial_upload_location = GURL(entry_proto->upload_url());

  upload_file_info->completion_callback =
      base::Bind(&GDataFileSystem::OnTransferCompleted,
                 ui_weak_ptr_,
                 params.callback);

  uploader_->UploadNewFile(upload_file_info.Pass());
}

void GDataFileSystem::OnTransferCompleted(
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info.get());

  if (error == GDATA_FILE_OK && upload_file_info->entry.get()) {
    AddUploadedFile(UPLOAD_NEW_FILE,
                    upload_file_info->gdata_path.DirName(),
                    upload_file_info->entry.Pass(),
                    upload_file_info->file_path,
                    GDataCache::FILE_OPERATION_COPY,
                    base::Bind(&OnAddUploadFileCompleted, callback, error));
  } else if (!callback.is_null()) {
    callback.Run(error);
  }
}

void GDataFileSystem::Copy(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  RunTaskOnUIThread(base::Bind(&GDataFileSystem::CopyOnUIThread,
                               ui_weak_ptr_,
                               src_file_path,
                               dest_file_path,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::CopyOnUIThread(const FilePath& src_file_path,
                                     const FilePath& dest_file_path,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  directory_service_->GetEntryInfoPairByPaths(
      src_file_path,
      dest_file_path.DirName(),
      base::Bind(&GDataFileSystem::CopyOnUIThreadAfterGetEntryInfoPair,
                 ui_weak_ptr_,
                 dest_file_path,
                 callback));
}

void GDataFileSystem::CopyOnUIThreadAfterGetEntryInfoPair(
    const FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  if (result->first.error != GDATA_FILE_OK) {
    callback.Run(result->first.error);
    return;
  } else if (result->second.error != GDATA_FILE_OK) {
    callback.Run(result->second.error);
    return;
  }

  scoped_ptr<GDataEntryProto> src_file_proto = result->first.proto.Pass();
  scoped_ptr<GDataEntryProto> dest_parent_proto = result->second.proto.Pass();

  if (!dest_parent_proto->file_info().is_directory()) {
    callback.Run(GDATA_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  } else if (src_file_proto->file_info().is_directory()) {
    // TODO(kochi): Implement copy for directories. In the interim,
    // we handle recursive directory copy in the file manager.
    // crbug.com/141596
    callback.Run(GDATA_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (src_file_proto->file_specific_info().is_hosted_document()) {
    CopyDocumentToDirectory(dest_file_path.DirName(),
                            src_file_proto->resource_id(),
                            // Drop the document extension, which should not be
                            // in the document title.
                            dest_file_path.BaseName().RemoveExtension().value(),
                            callback);
    return;
  }

  // TODO(kochi): Reimplement this once the server API supports
  // copying of regular files directly on the server side. crbug.com/138273
  const FilePath& src_file_path = result->first.path;
  GetFileByPath(src_file_path,
                base::Bind(&GDataFileSystem::OnGetFileCompleteForCopy,
                           ui_weak_ptr_,
                           dest_file_path,
                           callback),
                GetDownloadDataCallback());
}

void GDataFileSystem::OnGetFileCompleteForCopy(
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    GDataFileError error,
    const FilePath& local_file_path,
    const std::string& unused_mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != GDATA_FILE_OK) {
    callback.Run(error);
    return;
  }

  // This callback is only triggered for a regular file via Copy() and runs
  // on the same thread as Copy (IO thread). As TransferRegularFile must run
  // on the UI thread, we thus need to post a task to the UI thread.
  // Also, upon the completion of TransferRegularFile, we need to run |callback|
  // on the same thread as Copy (IO thread), and the transition from UI thread
  // to IO thread is handled by OnTransferRegularFileCompleteForCopy.
  DCHECK_EQ(REGULAR_FILE, file_type);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GDataFileSystem::TransferRegularFile,
                 ui_weak_ptr_,
                 local_file_path, remote_dest_file_path,
                 base::Bind(OnTransferRegularFileCompleteForCopy,
                            callback,
                            base::MessageLoopProxy::current())));
}

void GDataFileSystem::OnGetFileCompleteForTransferFile(
    const FilePath& local_dest_file_path,
    const FileOperationCallback& callback,
    GDataFileError error,
    const FilePath& local_file_path,
    const std::string& unused_mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != GDATA_FILE_OK) {
    callback.Run(error);
    return;
  }

  // GetFileByPath downloads the file from gdata to a local cache, which is then
  // copied to the actual destination path on the local file system using
  // CopyLocalFileOnBlockingPool.
  GDataFileError* copy_file_error =
      new GDataFileError(GDATA_FILE_OK);
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&CopyLocalFileOnBlockingPool,
                 local_file_path,
                 local_dest_file_path,
                 copy_file_error),
      base::Bind(&RunFileOperationCallbackHelper,
                 callback,
                 base::Owned(copy_file_error)));
}

void GDataFileSystem::CopyDocumentToDirectory(
    const FilePath& dir_path,
    const std::string& resource_id,
    const FilePath::StringType& new_name,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  documents_service_->CopyDocument(resource_id, new_name,
      base::Bind(&GDataFileSystem::OnCopyDocumentCompleted,
                 ui_weak_ptr_,
                 dir_path,
                 callback));
}

void GDataFileSystem::Rename(const FilePath& file_path,
                             const FilePath::StringType& new_name,
                             const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // It is a no-op if the file is renamed to the same name.
  if (file_path.BaseName().value() == new_name) {
    callback.Run(GDATA_FILE_OK, file_path);
    return;
  }

  // Get the edit URL of an entry at |file_path|.
  directory_service_->GetEntryInfoByPath(
      file_path,
      base::Bind(
          &GDataFileSystem::RenameAfterGetEntryInfo,
          ui_weak_ptr_,
          file_path,
          new_name,
          callback));
}

void GDataFileSystem::RenameAfterGetEntryInfo(
    const FilePath& file_path,
    const FilePath::StringType& new_name,
    const FileMoveCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, file_path);
    return;
  }
  DCHECK(entry_proto.get());

  // Drop the .g<something> extension from |new_name| if the file being
  // renamed is a hosted document and |new_name| has the same .g<something>
  // extension as the file.
  FilePath::StringType file_name = new_name;
  if (entry_proto->has_file_specific_info() &&
      entry_proto->file_specific_info().is_hosted_document()) {
    FilePath new_file(file_name);
    if (new_file.Extension() ==
        entry_proto->file_specific_info().document_extension()) {
      file_name = new_file.RemoveExtension().value();
    }
  }

  documents_service_->RenameResource(
      GURL(entry_proto->edit_url()),
      file_name,
      base::Bind(&GDataFileSystem::RenameFileOnFileSystem,
                 ui_weak_ptr_,
                 file_path,
                 file_name,
                 callback));
}

void GDataFileSystem::Move(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  RunTaskOnUIThread(base::Bind(&GDataFileSystem::MoveOnUIThread,
                               ui_weak_ptr_,
                               src_file_path,
                               dest_file_path,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::MoveOnUIThread(const FilePath& src_file_path,
                                     const FilePath& dest_file_path,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  directory_service_->GetEntryInfoPairByPaths(
      src_file_path,
      dest_file_path.DirName(),
      base::Bind(&GDataFileSystem::MoveOnUIThreadAfterGetEntryInfoPair,
                 ui_weak_ptr_,
                 dest_file_path,
                 callback));
}

void GDataFileSystem::MoveOnUIThreadAfterGetEntryInfoPair(
    const FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  if (result->first.error != GDATA_FILE_OK) {
    callback.Run(result->first.error);
    return;
  } else if (result->second.error != GDATA_FILE_OK) {
    callback.Run(result->second.error);
    return;
  }

  scoped_ptr<GDataEntryProto> dest_parent_proto = result->second.proto.Pass();
  if (!dest_parent_proto->file_info().is_directory()) {
    callback.Run(GDATA_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // If the file/directory is moved to the same directory, just rename it.
  const FilePath& src_file_path = result->first.path;
  const FilePath& dest_parent_path = result->second.path;
  if (src_file_path.DirName() == dest_parent_path) {
    FileMoveCallback final_file_path_update_callback =
        base::Bind(&GDataFileSystem::OnFilePathUpdated,
                   ui_weak_ptr_,
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
  FileMoveCallback add_file_to_directory_callback =
      base::Bind(&GDataFileSystem::MoveEntryFromRootDirectory,
                 ui_weak_ptr_,
                 dest_file_path.DirName(),
                 callback);

  FileMoveCallback remove_file_from_directory_callback =
      base::Bind(&GDataFileSystem::RemoveEntryFromDirectory,
                 ui_weak_ptr_,
                 src_file_path.DirName(),
                 add_file_to_directory_callback);

  Rename(src_file_path, dest_file_path.BaseName().value(),
         remove_file_from_directory_callback);
}

void GDataFileSystem::MoveEntryFromRootDirectory(
    const FilePath& dir_path,
    const FileOperationCallback& callback,
    GDataFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK_EQ(kGDataRootDirectory, file_path.DirName().value());

  GDataEntry* entry = directory_service_->FindEntryByPathSync(file_path);
  GDataEntry* dir_entry = directory_service_->FindEntryByPathSync(dir_path);
  if (error == GDATA_FILE_OK) {
    if (!entry || !dir_entry) {
      error = GDATA_FILE_ERROR_NOT_FOUND;
    } else {
      if (!dir_entry->AsGDataDirectory())
        error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
    }
  }

  // Returns if there is an error or |dir_path| is the root directory.
  if (error != GDATA_FILE_OK ||
      dir_entry->resource_id() == kGDataRootDirectoryResourceId) {
    callback.Run(error);
    return;
  }

  documents_service_->AddResourceToDirectory(
      dir_entry->content_url(),
      entry->edit_url(),
      base::Bind(&GDataFileSystem::OnMoveEntryFromRootDirectoryCompleted,
                 ui_weak_ptr_,
                 callback,
                 file_path,
                 dir_path));
}

void GDataFileSystem::RemoveEntryFromDirectory(
    const FilePath& dir_path,
    const FileMoveCallback& callback,
    GDataFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataEntry* entry = directory_service_->FindEntryByPathSync(file_path);
  GDataEntry* dir = directory_service_->FindEntryByPathSync(dir_path);
  if (error == GDATA_FILE_OK) {
    if (!entry || !dir) {
      error = GDATA_FILE_ERROR_NOT_FOUND;
    } else {
      if (!dir->AsGDataDirectory())
        error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
    }
  }

  // Returns if there is an error or |dir_path| is the root directory.
  if (error != GDATA_FILE_OK ||
      dir->resource_id() == kGDataRootDirectoryResourceId) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, error, file_path));
    }
    return;
  }

  documents_service_->RemoveResourceFromDirectory(
      dir->content_url(),
      entry->edit_url(),
      entry->resource_id(),
      base::Bind(&GDataFileSystem::RemoveEntryFromDirectoryOnFileSystem,
                 ui_weak_ptr_,
                 callback,
                 file_path,
                 dir_path));
}

void GDataFileSystem::Remove(const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::RemoveOnUIThread,
                               ui_weak_ptr_,
                               file_path,
                               is_recursive,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::RemoveOnUIThread(
    const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Get the edit URL of an entry at |file_path|.
  directory_service_->GetEntryInfoByPath(
      file_path,
      base::Bind(
          &GDataFileSystem::RemoveOnUIThreadAfterGetEntryInfo,
          ui_weak_ptr_,
          file_path,
          is_recursive,
          callback));
}

void GDataFileSystem::RemoveOnUIThreadAfterGetEntryInfo(
    const FilePath& file_path,
    bool /* is_recursive */,
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null()) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE, base::Bind(callback, error));
    }
    return;
  }

  DCHECK(entry_proto.get());
  documents_service_->DeleteDocument(
      GURL(entry_proto->edit_url()),
      base::Bind(&GDataFileSystem::OnRemovedDocument,
                 ui_weak_ptr_,
                 callback,
                 file_path));
}

void GDataFileSystem::CreateDirectory(
    const FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::CreateDirectoryOnUIThread,
                               ui_weak_ptr_,
                               directory_path,
                               is_exclusive,
                               is_recursive,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::CreateDirectoryOnUIThread(
    const FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
            base::Bind(callback, GDATA_FILE_ERROR_NOT_FOUND));
      }

      return;
    }
    case DIRECTORY_ALREADY_PRESENT: {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback,
                       is_exclusive ? GDATA_FILE_ERROR_EXISTS :
                                      GDATA_FILE_OK));
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
           base::Bind(callback, GDATA_FILE_ERROR_NOT_FOUND));
    }
    return;
  }

  documents_service_->CreateDirectory(
      last_parent_dir_url,
      first_missing_path.BaseName().value(),
      base::Bind(&GDataFileSystem::OnCreateDirectoryCompleted,
                 ui_weak_ptr_,
                 CreateDirectoryParams(
                     first_missing_path,
                     directory_path,
                     is_exclusive,
                     is_recursive,
                     callback)));
}

void GDataFileSystem::CreateFile(const FilePath& file_path,
                                 bool is_exclusive,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::CreateFileOnUIThread,
                               ui_weak_ptr_,
                               file_path,
                               is_exclusive,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::CreateFileOnUIThread(
    const FilePath& file_path,
    bool is_exclusive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // First, checks the existence of a file at |file_path|.
  FindEntryByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnGetEntryInfoForCreateFile,
                 ui_weak_ptr_,
                 file_path,
                 is_exclusive,
                 callback));
}

void GDataFileSystem::OnGetEntryInfoForCreateFile(
    const FilePath& file_path,
    bool is_exclusive,
    const FileOperationCallback& callback,
    GDataFileError result,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The |file_path| is invalid. It is an error.
  if (result != GDATA_FILE_ERROR_NOT_FOUND &&
      result != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(result);
    return;
  }

  // An entry already exists at |file_path|.
  if (result == GDATA_FILE_OK) {
    // If an exclusive mode is requested, or the entry is not a regular file,
    // it is an error.
    if (is_exclusive ||
        !entry->AsGDataFile() ||
        entry->AsGDataFile()->is_hosted_document()) {
      if (!callback.is_null())
        callback.Run(GDATA_FILE_ERROR_EXISTS);
      return;
    }

    // Otherwise nothing more to do. Succeeded.
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK);
    return;
  }

  // No entry found at |file_path|. Let's create a brand new file.
  // For now, it is implemented by uploading an empty file (/dev/null).
  // TODO(kinaba): http://crbug.com/135143. Implement in a nicer way.
  TransferRegularFile(FilePath(kEmptyFilePath), file_path, callback);
}

void GDataFileSystem::GetFileByPath(
    const FilePath& file_path,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::GetFileByPathOnUIThread,
                 ui_weak_ptr_,
                 file_path,
                 CreateRelayCallback(get_file_callback),
                 CreateRelayCallback(get_download_data_callback)));
}

void GDataFileSystem::GetFileByPathOnUIThread(
    const FilePath& file_path,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  directory_service_->GetEntryInfoByPath(
      file_path,
      base::Bind(&GDataFileSystem::OnGetEntryInfoCompleteForGetFileByPath,
                 ui_weak_ptr_,
                 file_path,
                 CreateRelayCallback(get_file_callback),
                 CreateRelayCallback(get_download_data_callback)));
}

void GDataFileSystem::OnGetEntryInfoCompleteForGetFileByPath(
    const FilePath& file_path,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If |error| == PLATFORM_FILE_OK then |entry_proto| must be valid.
  DCHECK(error != GDATA_FILE_OK ||
         (entry_proto.get() && !entry_proto->resource_id().empty()));
  GetResolvedFileByPath(file_path,
                        get_file_callback,
                        get_download_data_callback,
                        error,
                        entry_proto.get());
}

void GDataFileSystem::GetResolvedFileByPath(
    const FilePath& file_path,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback,
    GDataFileError error,
    const GDataEntryProto* entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_proto && !entry_proto->has_file_specific_info())
    error = GDATA_FILE_ERROR_NOT_FOUND;

  if (error != GDATA_FILE_OK) {
    if (!get_file_callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(get_file_callback,
                     GDATA_FILE_ERROR_NOT_FOUND,
                     FilePath(),
                     std::string(),
                     REGULAR_FILE));
    }
    return;
  }

  // For a hosted document, we create a special JSON file to represent the
  // document instead of fetching the document content in one of the exported
  // formats. The JSON file contains the edit URL and resource ID of the
  // document.
  if (entry_proto->file_specific_info().is_hosted_document()) {
    GDataFileError* error =
        new GDataFileError(GDATA_FILE_OK);
    FilePath* temp_file_path = new FilePath;
    std::string* mime_type = new std::string;
    GDataFileType* file_type = new GDataFileType(REGULAR_FILE);
    util::PostBlockingPoolSequencedTaskAndReply(
        FROM_HERE,
        blocking_task_runner_,
        base::Bind(&CreateDocumentJsonFileOnBlockingPool,
                   cache_->GetCacheDirectoryPath(
                       GDataCache::CACHE_TYPE_TMP_DOCUMENTS),
                   GURL(entry_proto->file_specific_info().alternate_url()),
                   entry_proto->resource_id(),
                   error,
                   temp_file_path,
                   mime_type,
                   file_type),
        base::Bind(&RunGetFileCallbackHelper,
                   get_file_callback,
                   base::Owned(error),
                   base::Owned(temp_file_path),
                   base::Owned(mime_type),
                   base::Owned(file_type)));
    return;
  }

  // Returns absolute path of the file if it were cached or to be cached.
  FilePath local_tmp_path = cache_->GetCacheFilePath(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5(),
      GDataCache::CACHE_TYPE_TMP,
      GDataCache::CACHED_FILE_FROM_SERVER);
  cache_->GetFileOnUIThread(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5(),
      base::Bind(
          &GDataFileSystem::OnGetFileFromCache,
          ui_weak_ptr_,
          GetFileFromCacheParams(
              file_path,
              local_tmp_path,
              GURL(entry_proto->content_url()),
              entry_proto->resource_id(),
              entry_proto->file_specific_info().file_md5(),
              entry_proto->file_specific_info().content_mime_type(),
              get_file_callback,
              get_download_data_callback)));
}

void GDataFileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::GetFileByResourceIdOnUIThread,
                 ui_weak_ptr_,
                 resource_id,
                 CreateRelayCallback(get_file_callback),
                 CreateRelayCallback(get_download_data_callback)));
}

void GDataFileSystem::GetFileByResourceIdOnUIThread(
    const std::string& resource_id,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  directory_service_->GetEntryByResourceIdAsync(resource_id,
      base::Bind(&GDataFileSystem::GetFileByEntryOnUIThread,
                 ui_weak_ptr_,
                 get_file_callback,
                 get_download_data_callback));
}

void GDataFileSystem::GetFileByEntryOnUIThread(
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath file_path;
  if (entry) {
    GDataFile* file = entry->AsGDataFile();
    if (file)
      file_path = file->GetFilePath();
  }

  // Report an error immediately if the file for the resource ID is not
  // found.
  if (file_path.empty()) {
    if (!get_file_callback.is_null()) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(get_file_callback,
                     GDATA_FILE_ERROR_NOT_FOUND,
                     FilePath(),
                     std::string(),
                     REGULAR_FILE));
    }
    return;
  }

  GetFileByPath(file_path, get_file_callback, get_download_data_callback);
}

void GDataFileSystem::OnGetFileFromCache(const GetFileFromCacheParams& params,
                                         GDataFileError error,
                                         const std::string& resource_id,
                                         const std::string& md5,
                                         const FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Have we found the file in cache? If so, return it back to the caller.
  if (error == GDATA_FILE_OK) {
    if (!params.get_file_callback.is_null()) {
      params.get_file_callback.Run(error,
                                   cache_file_path,
                                   params.mime_type,
                                   REGULAR_FILE);
    }
    return;
  }

  // If cache file is not found, try to download the file from the server
  // instead. This logic is rather complicated but here's how this works:
  //
  // Retrieve fresh file metadata from server. We will extract file size and
  // content url from there (we want to make sure used content url is not
  // stale).
  //
  // Check if we have enough space, based on the expected file size.
  // - if we don't have enough space, try to free up the disk space
  // - if we still don't have enough space, return "no space" error
  // - if we have enough space, start downloading the file from the server
  documents_service_->GetDocumentEntry(
      resource_id,
      base::Bind(&GDataFileSystem::OnGetDocumentEntry,
                 ui_weak_ptr_,
                 cache_file_path,
                 GetFileFromCacheParams(params.virtual_file_path,
                                        params.local_tmp_path,
                                        params.content_url,
                                        params.resource_id,
                                        params.md5,
                                        params.mime_type,
                                        params.get_file_callback,
                                        params.get_download_data_callback)));
}

void GDataFileSystem::OnGetDocumentEntry(const FilePath& cache_file_path,
                                         const GetFileFromCacheParams& params,
                                         GDataErrorCode status,
                                         scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError error = util::GDataToGDataFileError(status);

  scoped_ptr<GDataEntry> fresh_entry;
  if (error == GDATA_FILE_OK) {
    scoped_ptr<DocumentEntry> doc_entry(DocumentEntry::ExtractAndParse(*data));
    if (doc_entry.get()) {
      fresh_entry.reset(directory_service_->FromDocumentEntry(doc_entry.get()));
    }
    if (!fresh_entry.get() || !fresh_entry->AsGDataFile()) {
      LOG(ERROR) << "Got invalid entry from server for " << params.resource_id;
      error = GDATA_FILE_ERROR_FAILED;
    }
  }

  if (error != GDATA_FILE_OK) {
    if (!params.get_file_callback.is_null()) {
      params.get_file_callback.Run(error,
                                   cache_file_path,
                                   params.mime_type,
                                   REGULAR_FILE);
    }
    return;
  }

  GURL content_url = fresh_entry->content_url();
  int64 file_size = fresh_entry->file_info().size;

  DCHECK_EQ(params.resource_id, fresh_entry->resource_id());
  scoped_ptr<GDataFile> fresh_entry_as_file(
      fresh_entry.release()->AsGDataFile());
  directory_service_->RefreshFile(fresh_entry_as_file.Pass());

  bool* has_enough_space = new bool(false);
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GDataCache::FreeDiskSpaceIfNeededFor,
                 base::Unretained(cache_),
                 file_size,
                 has_enough_space),
      base::Bind(&GDataFileSystem::StartDownloadFileIfEnoughSpace,
                 ui_weak_ptr_,
                 params,
                 content_url,
                 cache_file_path,
                 base::Owned(has_enough_space)));
}

void GDataFileSystem::StartDownloadFileIfEnoughSpace(
    const GetFileFromCacheParams& params,
    const GURL& content_url,
    const FilePath& cache_file_path,
    bool* has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!*has_enough_space) {
    // If no enough space, return PLATFORM_FILE_ERROR_NO_SPACE.
    if (!params.get_file_callback.is_null()) {
      params.get_file_callback.Run(GDATA_FILE_ERROR_NO_SPACE,
                                   cache_file_path,
                                   params.mime_type,
                                   REGULAR_FILE);
    }
    return;
  }

  // We have enough disk space. Start downloading the file.
  documents_service_->DownloadFile(
      params.virtual_file_path,
      params.local_tmp_path,
      content_url,
      base::Bind(&GDataFileSystem::OnFileDownloaded,
                 ui_weak_ptr_,
                 params),
      params.get_download_data_callback);
}

void GDataFileSystem::GetEntryInfoByPath(const FilePath& file_path,
                                         const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::GetEntryInfoByPathAsyncOnUIThread,
                 ui_weak_ptr_,
                 file_path,
                 CreateRelayCallback(callback)));
}

void GDataFileSystem::GetEntryInfoByPathAsyncOnUIThread(
    const FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FindEntryByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnGetEntryInfo,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnGetEntryInfo(const GetEntryInfoCallback& callback,
                                    GDataFileError error,
                                    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, scoped_ptr<GDataEntryProto>());
    return;
  }
  DCHECK(entry);

  scoped_ptr<GDataEntryProto> entry_proto(new GDataEntryProto);
  entry->ToProtoFull(entry_proto.get());

  CheckLocalModificationAndRun(entry_proto.Pass(), callback);
}

void GDataFileSystem::ReadDirectoryByPath(
    const FilePath& file_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::ReadDirectoryByPathAsyncOnUIThread,
                 ui_weak_ptr_,
                 file_path,
                 CreateRelayCallback(callback)));
}

void GDataFileSystem::ReadDirectoryByPathAsyncOnUIThread(
    const FilePath& file_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FindEntryByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnReadDirectory,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnReadDirectory(
    const ReadDirectoryWithSettingCallback& callback,
    GDataFileError error,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error,
                   hide_hosted_docs_,
                   scoped_ptr<GDataEntryProtoVector>());
    return;
  }
  DCHECK(entry);

  GDataDirectory* directory = entry->AsGDataDirectory();
  if (!directory) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND,
                   hide_hosted_docs_,
                   scoped_ptr<GDataEntryProtoVector>());
    return;
  }

  scoped_ptr<GDataEntryProtoVector> entries(directory->ToProtoVector());

  if (!callback.is_null())
    callback.Run(GDATA_FILE_OK, hide_hosted_docs_, entries.Pass());
}

void GDataFileSystem::RequestDirectoryRefresh(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::RequestDirectoryRefreshOnUIThread,
                 ui_weak_ptr_,
                 file_path));
}

void GDataFileSystem::RequestDirectoryRefreshOnUIThread(
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Make sure the destination directory exists.
  directory_service_->GetEntryInfoByPath(
      file_path,
      base::Bind(
          &GDataFileSystem::RequestDirectoryRefreshOnUIThreadAfterGetEntryInfo,
          ui_weak_ptr_,
          file_path));
}

void GDataFileSystem::RequestDirectoryRefreshOnUIThreadAfterGetEntryInfo(
    const FilePath& file_path,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK ||
      !entry_proto->file_info().is_directory()) {
    LOG(ERROR) << "Directory entry not found: " << file_path.value();
    return;
  }

  feed_loader_->LoadFromServer(
      directory_service_->origin(),
      0,  // Not delta feed.
      0,  // Not used.
      true,  // multiple feeds
      file_path,
      std::string(),  // No search query
      GURL(), /* feed not explicitly set */
      entry_proto->resource_id(),
      FindEntryCallback(),  // Not used.
      base::Bind(&GDataFileSystem::OnRequestDirectoryRefresh,
                 ui_weak_ptr_));
}

void GDataFileSystem::OnRequestDirectoryRefresh(
    GetDocumentsParams* params,
    GDataFileError error) {
  DCHECK(params);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FilePath& directory_path = params->search_file_path;
  if (error != GDATA_FILE_OK) {
    LOG(ERROR) << "Failed to refresh directory: " << directory_path.value()
               << ": " << error;
    return;
  }

  int64 unused_delta_feed_changestamp = 0;
  FeedToFileResourceMapUmaStats unused_uma_stats;
  FileResourceIdMap file_map;
  GDataWapiFeedProcessor feed_processor(directory_service_.get());
  error = feed_processor.FeedToFileResourceMap(
      *params->feed_list,
      &file_map,
      &unused_delta_feed_changestamp,
      &unused_uma_stats);
  if (error != GDATA_FILE_OK) {
    LOG(ERROR) << "Failed to convert feed: " << directory_path.value()
               << ": " << error;
    return;
  }

  directory_service_->GetEntryByResourceIdAsync(params->directory_resource_id,
      base::Bind(&GDataFileSystem::RequestDirectoryRefreshByEntry,
                 ui_weak_ptr_,
                 directory_path,
                 params->directory_resource_id,
                 file_map));
}

void GDataFileSystem::RequestDirectoryRefreshByEntry(
    const FilePath& directory_path,
    const std::string& directory_resource_id,
    const FileResourceIdMap& file_map,
    GDataEntry* directory_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!directory_entry || !directory_entry->AsGDataDirectory()) {
    LOG(ERROR) << "Directory entry is gone: " << directory_path.value()
               << ": " << directory_resource_id;
    return;
  }
  GDataDirectory* directory = directory_entry->AsGDataDirectory();

  // Remove the existing files.
  directory->RemoveChildFiles();
  // Go through all entries generated by the feed and add files.
  for (FileResourceIdMap::const_iterator it = file_map.begin();
       it != file_map.end(); ++it) {
    scoped_ptr<GDataEntry> entry(it->second);
    // Skip if it's not a file (i.e. directory).
    if (!entry->AsGDataFile())
      continue;
    directory->AddEntry(entry.release());
  }

  // Note that there may be no change in the directory, but it's expensive to
  // check if the new metadata matches the existing one, so we just always
  // notify that the directory is changed.
  OnDirectoryChanged(directory_path);
  DVLOG(1) << "Directory refreshed: " << directory_path.value();
}

void GDataFileSystem::UpdateFileByResourceId(
    const std::string& resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(
      base::Bind(&GDataFileSystem::UpdateFileByResourceIdOnUIThread,
                 ui_weak_ptr_,
                 resource_id,
                 CreateRelayCallback(callback)));
}

void GDataFileSystem::UpdateFileByResourceIdOnUIThread(
    const std::string& resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  directory_service_->GetEntryByResourceIdAsync(resource_id,
      base::Bind(&GDataFileSystem::UpdateFileByEntryOnUIThread,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::UpdateFileByEntryOnUIThread(
    const FileOperationCallback& callback,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!entry || !entry->AsGDataFile()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_FILE_ERROR_NOT_FOUND));
    return;
  }
  GDataFile* file = entry->AsGDataFile();

  cache_->GetFileOnUIThread(
      file->resource_id(),
      file->file_md5(),
      base::Bind(&GDataFileSystem::OnGetFileCompleteForUpdateFile,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnGetFileCompleteForUpdateFile(
    const FileOperationCallback& callback,
    GDataFileError error,
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error);
    return;
  }

  // Gets the size of the cache file. Since the file is locally modified, the
  // file size information stored in GDataEntry is not correct.
  GDataFileError* get_size_error = new GDataFileError(GDATA_FILE_ERROR_FAILED);
  int64* file_size = new int64(-1);
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GetLocalFileSizeOnBlockingPool,
                 cache_file_path,
                 get_size_error,
                 file_size),
      base::Bind(&GDataFileSystem::OnGetFileSizeCompleteForUpdateFile,
                 ui_weak_ptr_,
                 callback,
                 resource_id,
                 md5,
                 cache_file_path,
                 base::Owned(get_size_error),
                 base::Owned(file_size)));
}

void GDataFileSystem::OnGetFileSizeCompleteForUpdateFile(
    const FileOperationCallback& callback,
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& cache_file_path,
    GDataFileError* error,
    int64* file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (*error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(*error);
    return;
  }

  directory_service_->GetEntryByResourceIdAsync(resource_id,
      base::Bind(&GDataFileSystem::OnGetFileCompleteForUpdateFileByEntry,
          ui_weak_ptr_,
          callback,
          md5,
          *file_size,
          cache_file_path));
}

void GDataFileSystem::OnGetFileCompleteForUpdateFileByEntry(
    const FileOperationCallback& callback,
    const std::string& md5,
    int64 file_size,
    const FilePath& cache_file_path,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!entry || !entry->AsGDataFile()) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND);
    return;
  }
  GDataFile* file = entry->AsGDataFile();

  uploader_->UploadExistingFile(
      file->upload_url(),
      file->GetFilePath(),
      cache_file_path,
      file_size,
      file->content_mime_type(),
      base::Bind(&GDataFileSystem::OnUpdatedFileUploaded,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnUpdatedFileUploaded(
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info.get());

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error);
    return;
  }

  AddUploadedFile(UPLOAD_EXISTING_FILE,
                  upload_file_info->gdata_path.DirName(),
                  upload_file_info->entry.Pass(),
                  upload_file_info->file_path,
                  GDataCache::FILE_OPERATION_MOVE,
                  base::Bind(&OnAddUploadFileCompleted, callback, error));
}

void GDataFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::GetAvailableSpaceOnUIThread,
                               ui_weak_ptr_,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::GetAvailableSpaceOnUIThread(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (gdata::util::IsDriveV2ApiEnabled()) {
    documents_service_->GetAboutResource(
      base::Bind(&GDataFileSystem::OnGetAboutResource,
                 ui_weak_ptr_,
                 callback));
    return;
  }

  documents_service_->GetAccountMetadata(
      base::Bind(&GDataFileSystem::OnGetAvailableSpace,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnGetAvailableSpace(
    const GetAvailableSpaceCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    callback.Run(error, -1, -1);
    return;
  }

  scoped_ptr<AccountMetadataFeed> feed;
  if (data.get())
    feed = AccountMetadataFeed::CreateFrom(*data);
  if (!feed.get()) {
    callback.Run(GDATA_FILE_ERROR_FAILED, -1, -1);
    return;
  }

  callback.Run(GDATA_FILE_OK,
               feed->quota_bytes_total(),
               feed->quota_bytes_used());
}

void GDataFileSystem::OnGetAboutResource(
    const GetAvailableSpaceCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> resource_json) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    callback.Run(error, -1, -1);
    return;
  }

  scoped_ptr<AboutResource> about;
  if (resource_json.get())
    about = AboutResource::CreateFrom(*resource_json);

  if (!about.get()) {
    callback.Run(GDATA_FILE_ERROR_FAILED, -1, -1);
    return;
  }

  callback.Run(GDATA_FILE_OK,
               about->quota_bytes_total(),
               about->quota_bytes_used());
}

void GDataFileSystem::OnCreateDirectoryCompleted(
    const CreateDirectoryParams& params,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(error);

    return;
  }

  base::DictionaryValue* dict_value = NULL;
  base::Value* created_entry = NULL;
  if (data.get() && data->GetAsDictionary(&dict_value) && dict_value)
    dict_value->Get("entry", &created_entry);
  error = AddNewDirectory(params.created_directory_path.DirName(),
                          created_entry);

  if (error != GDATA_FILE_OK) {
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
    params.callback.Run(GDATA_FILE_OK);
  }
}

void GDataFileSystem::OnSearch(const SearchCallback& callback,
                               GetDocumentsParams* params,
                               GDataFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  // The search results will be returned using virtual directory.
  // The directory is not really part of the file system, so it has no parent or
  // root.
  std::vector<SearchResultInfo>* results(new std::vector<SearchResultInfo>());

  DCHECK_EQ(1u, params->feed_list->size());
  DocumentFeed* feed = params->feed_list->at(0);

  // TODO(tbarzic): Limit total number of returned results for the query.
  GURL next_feed;
  feed->GetNextFeedURL(&next_feed);

  if (feed->entries().empty()) {
    scoped_ptr<std::vector<SearchResultInfo> > result_vec(results);
    if (!callback.is_null())
      callback.Run(error, next_feed, result_vec.Pass());
    return;
  }

  // Go through all entires generated by the feed and add them to the search
  // result directory.
  for (size_t i = 0; i < feed->entries().size(); ++i) {
    DocumentEntry* doc = const_cast<DocumentEntry*>(feed->entries()[i]);
    scoped_ptr<GDataEntry> entry(directory_service_->FromDocumentEntry(doc));

    if (!entry.get())
      continue;

    DCHECK_EQ(doc->resource_id(), entry->resource_id());
    DCHECK(!entry->is_deleted());

    std::string entry_resource_id = entry->resource_id();

    // This will do nothing if the entry is not already present in file system.
    if (entry->AsGDataFile()) {
      scoped_ptr<GDataFile> entry_as_file(entry.release()->AsGDataFile());
      directory_service_->RefreshFile(entry_as_file.Pass());
      // We shouldn't use entry object after this point.
      DCHECK(!entry.get());
    }

    // We will need information about result entry to create info for callback.
    // We can't use |entry| anymore, so we have to refetch entry from file
    // system. Also, |entry| doesn't have file path set before |RefreshFile|
    // call, so we can't get file path from there.
    directory_service_->GetEntryByResourceIdAsync(entry_resource_id,
        base::Bind(&AddEntryToSearchResults,
                   results,
                   callback,
                   base::Bind(&GDataFileSystem::CheckForUpdates, ui_weak_ptr_),
                   error,
                   i+1 == feed->entries().size(),
                   next_feed));
  }
}

void GDataFileSystem::Search(const std::string& search_query,
                             const GURL& next_feed,
                             const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::SearchAsyncOnUIThread,
                               ui_weak_ptr_,
                               search_query,
                               next_feed,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::SearchAsyncOnUIThread(
    const std::string& search_query,
    const GURL& next_feed,
    const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<std::vector<DocumentFeed*> > feed_list(
      new std::vector<DocumentFeed*>);

  ContentOrigin initial_origin = directory_service_->origin();
  feed_loader_->LoadFromServer(
      initial_origin,
      0, 0,  // We don't use change stamps when fetching search
      // data; we always fetch the whole result feed.
      false,  // Stop fetching search results after first feed
      // chunk to avoid displaying huge number of search
      // results (especially since we don't cache them).
      FilePath(),  // Not used.
      search_query,
      next_feed,
      std::string(),  // No directory resource ID.
      FindEntryCallback(),  // Not used.
      base::Bind(&GDataFileSystem::OnSearch, ui_weak_ptr_, callback));
}

void GDataFileSystem::OnDirectoryChanged(const FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnDirectoryChanged(directory_path));
}

void GDataFileSystem::OnDocumentFeedFetched(int num_accumulated_entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnDocumentFeedFetched(num_accumulated_entries));
}

void GDataFileSystem::OnFeedFromServerLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnFeedFromServerLoaded());
}

void GDataFileSystem::LoadRootFeedFromCacheForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  feed_loader_->LoadFromCache(
      false,  // should_load_from_server.
      // search_path doesn't matter if FindEntryCallback parameter is null .
      FilePath(),
      FindEntryCallback());
}

GDataFileError GDataFileSystem::UpdateFromFeedForTesting(
    const std::vector<DocumentFeed*>& feed_list,
    int64 start_changestamp,
    int64 root_feed_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return feed_loader_->UpdateFromFeed(feed_list,
                                      start_changestamp,
                                      root_feed_changestamp);
}

void GDataFileSystem::OnFilePathUpdated(const FileOperationCallback& callback,
                                        GDataFileError error,
                                        const FilePath& /* file_path */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null())
    callback.Run(error);
}

void GDataFileSystem::OnCopyDocumentCompleted(
    const FilePath& dir_path,
    const FileOperationCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    callback.Run(error);
    return;
  }

  scoped_ptr<DocumentEntry> doc_entry(DocumentEntry::ExtractAndParse(*data));
  if (!doc_entry.get()) {
    callback.Run(GDATA_FILE_ERROR_FAILED);
    return;
  }

  GDataEntry* entry = directory_service_->FromDocumentEntry(doc_entry.get());
  if (!entry) {
    callback.Run(GDATA_FILE_ERROR_FAILED);
    return;
  }

  // |entry| was added in the root directory on the server, so we should
  // first add it to |root_| to mirror the state and then move it to the
  // destination directory by MoveEntryFromRootDirectory().
  directory_service_->root()->AddEntry(entry);
  MoveEntryFromRootDirectory(dir_path,
                             callback,
                             GDATA_FILE_OK,
                             entry->GetFilePath());
}

void GDataFileSystem::OnMoveEntryFromRootDirectoryCompleted(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    const FilePath& dir_path,
    GDataErrorCode status,
    const GURL& document_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error == GDATA_FILE_OK) {
    GDataEntry* entry = directory_service_->FindEntryByPathSync(file_path);
    if (entry) {
      DCHECK_EQ(directory_service_->root(), entry->parent());
      directory_service_->MoveEntryToDirectory(dir_path, entry,
          base::Bind(
              &GDataFileSystem::OnMoveEntryToDirectoryWithFileOperationCallback,
              ui_weak_ptr_,
              callback));
      return;
    } else {
      error = GDATA_FILE_ERROR_NOT_FOUND;
    }
  }

  callback.Run(error);
}

void GDataFileSystem::OnRemovedDocument(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    GDataErrorCode status,
    const GURL& document_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError error = util::GDataToGDataFileError(status);

  if (error == GDATA_FILE_OK)
    error = RemoveEntryFromFileSystem(file_path);

  if (!callback.is_null()) {
    callback.Run(error);
  }
}

void GDataFileSystem::OnFileDownloaded(
    const GetFileFromCacheParams& params,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& downloaded_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If user cancels download of a pinned-but-not-fetched file, mark file as
  // unpinned so that we do not sync the file again.
  if (status == GDATA_CANCELLED) {
    cache_->GetCacheEntryOnUIThread(
        params.resource_id,
        params.md5,
        base::Bind(&GDataFileSystem::UnpinIfPinned,
                   ui_weak_ptr_,
                   params.resource_id,
                   params.md5));
  }

  // At this point, the disk can be full or nearly full for several reasons:
  // - The expected file size was incorrect and the file was larger
  // - There was an in-flight download operation and it used up space
  // - The disk became full for some user actions we cannot control
  //   (ex. the user might have downloaded a large file from a regular web site)
  //
  // If we don't have enough space, we return PLATFORM_FILE_ERROR_NO_SPACE,
  // and try to free up space, even if the file was downloaded successfully.
  bool* has_enough_space = new bool(false);
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GDataCache::FreeDiskSpaceIfNeededFor,
                 base::Unretained(cache_),
                 0,
                 has_enough_space),
      base::Bind(&GDataFileSystem::OnFileDownloadedAndSpaceChecked,
                 ui_weak_ptr_,
                 params,
                 status,
                 content_url,
                 downloaded_file_path,
                 base::Owned(has_enough_space)));
}

void GDataFileSystem::UnpinIfPinned(
    const std::string& resource_id,
    const std::string& md5,
    bool success,
    const GDataCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(hshi): http://crbug.com/127138 notify when file properties change.
  // This allows file manager to clear the "Available offline" checkbox.
  if (success && cache_entry.is_pinned())
    cache_->UnpinOnUIThread(resource_id, md5, CacheOperationCallback());
}

void GDataFileSystem::OnFileDownloadedAndSpaceChecked(
    const GetFileFromCacheParams& params,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& downloaded_file_path,
    bool* has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError error = util::GDataToGDataFileError(status);

  // Make sure that downloaded file is properly stored in cache. We don't have
  // to wait for this operation to finish since the user can already use the
  // downloaded file.
  if (error == GDATA_FILE_OK) {
    if (*has_enough_space) {
      cache_->StoreOnUIThread(
          params.resource_id,
          params.md5,
          downloaded_file_path,
          GDataCache::FILE_OPERATION_MOVE,
          base::Bind(&GDataFileSystem::OnDownloadStoredToCache,
                     ui_weak_ptr_));
    } else {
      // If we don't have enough space, remove the downloaded file, and
      // report "no space" error.
      util::PostBlockingPoolSequencedTask(
          FROM_HERE,
          blocking_task_runner_,
          base::Bind(base::IgnoreResult(&file_util::Delete),
                     downloaded_file_path,
                     false /* recursive*/));
      error = GDATA_FILE_ERROR_NO_SPACE;
    }
  }

  if (!params.get_file_callback.is_null()) {
    params.get_file_callback.Run(error,
                                 downloaded_file_path,
                                 params.mime_type,
                                 REGULAR_FILE);
  }
}

void GDataFileSystem::OnDownloadStoredToCache(GDataFileError error,
                                              const std::string& resource_id,
                                              const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Nothing much to do here for now.
}

void GDataFileSystem::RenameFileOnFileSystem(
    const FilePath& file_path,
    const FilePath::StringType& new_name,
    const FileMoveCallback& callback,
    GDataErrorCode status,
    const GURL& document_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, FilePath());
    return;
  }

  GDataEntry* entry = directory_service_->FindEntryByPathSync(file_path);
  if (!entry) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND, FilePath());
    return;
  }

  DCHECK(entry->parent());
  entry->set_title(new_name);
  // After changing the title of the entry, call MoveEntryToDirectory() to
  // remove the entry from its parent directory and then add it back in order to
  // go through the file name de-duplication.
  // TODO(achuith/satorux/zel): This code is fragile. The title has been
  // changed, but not the file_name. MoveEntryToDirectory calls RemoveChild to
  // remove the child based on the old file_name, and then re-adds the child by
  // first assigning the new title to file_name. http://crbug.com/30157
  directory_service_->MoveEntryToDirectory(
      entry->parent()->GetFilePath(),
      entry,
      base::Bind(&GDataFileSystem::OnMoveEntryToDirectoryWithFileMoveCallback,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::RemoveEntryFromDirectoryOnFileSystem(
    const FileMoveCallback& callback,
    const FilePath& file_path,
    const FilePath& dir_path,
    GDataErrorCode status,
    const GURL& document_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, FilePath());
    return;
  }

  GDataEntry* entry = directory_service_->FindEntryByPathSync(file_path);
  if (!entry) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND, FilePath());
    return;
  }

  directory_service_->MoveEntryToDirectory(
      directory_service_->root()->GetFilePath(),
      entry,
      base::Bind(&GDataFileSystem::OnMoveEntryToDirectoryWithFileMoveCallback,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnMoveEntryToDirectoryWithFileMoveCallback(
    const FileMoveCallback& callback,
    GDataFileError error,
    const FilePath& moved_file_path) {
  if (error == GDATA_FILE_OK)
    OnDirectoryChanged(moved_file_path.DirName());

  if (!callback.is_null())
    callback.Run(error, moved_file_path);
}

void GDataFileSystem::OnMoveEntryToDirectoryWithFileOperationCallback(
    const FileOperationCallback& callback,
    GDataFileError error,
    const FilePath& moved_file_path) {
  DCHECK(!callback.is_null());

  if (error == GDATA_FILE_OK)
    OnDirectoryChanged(moved_file_path.DirName());

  callback.Run(error);
}

GDataFileError GDataFileSystem::RemoveEntryFromFileSystem(
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string resource_id;
  GDataFileError error = RemoveEntryFromGData(file_path, &resource_id);
  if (error != GDATA_FILE_OK)
    return error;

  // If resource_id is not empty, remove its corresponding file from cache.
  if (!resource_id.empty())
    cache_->RemoveOnUIThread(resource_id, CacheOperationCallback());

  return GDATA_FILE_OK;
}

// static
void GDataFileSystem::RemoveStaleEntryOnUpload(const std::string& resource_id,
                                               GDataDirectory* parent_dir,
                                               GDataEntry* existing_entry) {
  if (existing_entry &&
      // This should always match, but just in case.
      existing_entry->parent() == parent_dir) {
    parent_dir->RemoveEntry(existing_entry);
  } else {
    LOG(ERROR) << "Entry for the existing file not found: " << resource_id;
  }
}

void GDataFileSystem::NotifyFileSystemMounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "File System is mounted";
  // Notify the observers that the file system is mounted.
  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnFileSystemMounted());
}

void GDataFileSystem::NotifyFileSystemToBeUnmounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "File System is to be unmounted";
  // Notify the observers that the file system is being unmounted.
  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnFileSystemBeingUnmounted());
}

void GDataFileSystem::RunAndNotifyInitialLoadFinished(
    const FindEntryCallback& callback,
    GDataFileError error,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!callback.is_null())
    callback.Run(error, entry);

  DVLOG(1) << "RunAndNotifyInitialLoadFinished";

  // Notify the observers that root directory has been initialized.
  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnInitialLoadFinished());
}

GDataFileError GDataFileSystem::AddNewDirectory(
    const FilePath& directory_path, base::Value* entry_value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!entry_value)
    return GDATA_FILE_ERROR_FAILED;

  scoped_ptr<DocumentEntry> doc_entry(DocumentEntry::CreateFrom(*entry_value));

  if (!doc_entry.get())
    return GDATA_FILE_ERROR_FAILED;

  // Find parent directory element within the cached file system snapshot.
  GDataEntry* entry = directory_service_->FindEntryByPathSync(directory_path);
  if (!entry)
    return GDATA_FILE_ERROR_FAILED;

  // Check if parent is a directory since in theory since this is a callback
  // something could in the meantime have nuked the parent dir and created a
  // file with the exact same name.
  GDataDirectory* parent_dir = entry->AsGDataDirectory();
  if (!parent_dir)
    return GDATA_FILE_ERROR_FAILED;

  GDataEntry* new_entry =
      directory_service_->FromDocumentEntry(doc_entry.get());
  if (!new_entry)
    return GDATA_FILE_ERROR_FAILED;

  parent_dir->AddEntry(new_entry);

  OnDirectoryChanged(directory_path);
  return GDATA_FILE_OK;
}

GDataFileSystem::FindMissingDirectoryResult
GDataFileSystem::FindFirstMissingParentDirectory(
    const FilePath& directory_path,
    GURL* last_dir_content_url,
    FilePath* first_missing_parent_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Let's find which how deep is the existing directory structure and
  // get the first element that's missing.
  std::vector<FilePath::StringType> path_parts;
  directory_path.GetComponents(&path_parts);
  FilePath current_path;

  for (std::vector<FilePath::StringType>::const_iterator iter =
          path_parts.begin();
       iter != path_parts.end(); ++iter) {
    current_path = current_path.Append(*iter);
    GDataEntry* entry = directory_service_->FindEntryByPathSync(current_path);
    if (entry) {
      if (entry->file_info().is_directory) {
        *last_dir_content_url = entry->content_url();
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

GDataFileError GDataFileSystem::RemoveEntryFromGData(
    const FilePath& file_path, std::string* resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  resource_id->clear();

  // Find directory element within the cached file system snapshot.
  GDataEntry* entry = directory_service_->FindEntryByPathSync(file_path);

  if (!entry)
    return GDATA_FILE_ERROR_NOT_FOUND;

  // You can't remove root element.
  if (!entry->parent())
    return GDATA_FILE_ERROR_ACCESS_DENIED;

  // If it's a file (only files have resource id), get its resource id so that
  // we can remove it after releasing the auto lock.
  if (entry->AsGDataFile())
    *resource_id = entry->AsGDataFile()->resource_id();

  GDataDirectory* parent_dir = entry->parent();
  parent_dir->RemoveEntry(entry);

  OnDirectoryChanged(parent_dir->GetFilePath());
  return GDATA_FILE_OK;
}

void GDataFileSystem::AddUploadedFile(
    UploadMode upload_mode,
    const FilePath& virtual_dir_path,
    scoped_ptr<DocumentEntry> entry,
    const FilePath& file_content_path,
    GDataCache::FileOperationType cache_operation,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Post a task to the same thread, rather than calling it here, as
  // AddUploadedFile() is asynchronous.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&GDataFileSystem::AddUploadedFileOnUIThread,
                 ui_weak_ptr_,
                 upload_mode,
                 virtual_dir_path,
                 base::Passed(&entry),
                 file_content_path,
                 cache_operation,
                 callback));
}

void GDataFileSystem::AddUploadedFileOnUIThread(
    UploadMode upload_mode,
    const FilePath& virtual_dir_path,
    scoped_ptr<DocumentEntry> entry,
    const FilePath& file_content_path,
    GDataCache::FileOperationType cache_operation,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // ScopedClosureRunner ensures that the specified callback is always invoked
  // upon return or passed on.
  base::ScopedClosureRunner callback_runner(callback);

  if (!entry.get()) {
    NOTREACHED();
    return;
  }

  GDataEntry* dir_entry = directory_service_->FindEntryByPathSync(
      virtual_dir_path);
  if (!dir_entry)
    return;

  GDataDirectory* parent_dir  = dir_entry->AsGDataDirectory();
  if (!parent_dir)
    return;

  scoped_ptr<GDataEntry> new_entry(
      directory_service_->FromDocumentEntry(entry.get()));
  if (!new_entry.get())
    return;

  if (upload_mode == UPLOAD_EXISTING_FILE) {
    // Remove an existing entry, which should be present.
    const std::string& resource_id = new_entry->resource_id();
    directory_service_->GetEntryByResourceIdAsync(resource_id,
        base::Bind(&RemoveStaleEntryOnUpload, resource_id, parent_dir));
  }

  GDataFile* file = new_entry->AsGDataFile();
  DCHECK(file);
  const std::string& resource_id = file->resource_id();
  const std::string& md5 = file->file_md5();
  parent_dir->AddEntry(new_entry.release());

  OnDirectoryChanged(virtual_dir_path);

  if (upload_mode == UPLOAD_NEW_FILE) {
    // Add the file to the cache if we have uploaded a new file.
    cache_->StoreOnUIThread(resource_id,
                            md5,
                            file_content_path,
                            cache_operation,
                            base::Bind(&OnCacheUpdatedForAddUploadedFile,
                                       callback_runner.Release()));
  } else if (upload_mode == UPLOAD_EXISTING_FILE) {
    // Clear the dirty bit if we have updated an existing file.
    cache_->ClearDirtyOnUIThread(resource_id,
                                 md5,
                                 base::Bind(&OnCacheUpdatedForAddUploadedFile,
                                            callback_runner.Release()));
  } else {
    NOTREACHED() << "Unexpected upload mode: " << upload_mode;
  }
}

void GDataFileSystem::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    PrefService* pref_service = profile_->GetPrefs();
    std::string* pref_name = content::Details<std::string>(details).ptr();
    if (*pref_name == prefs::kDisableGDataHostedFiles) {
      SetHideHostedDocuments(
          pref_service->GetBoolean(prefs::kDisableGDataHostedFiles));
    }
  } else {
    NOTREACHED();
  }
}

void GDataFileSystem::SetHideHostedDocuments(bool hide) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (hide == hide_hosted_docs_)
    return;

  hide_hosted_docs_ = hide;
  const FilePath root_path = directory_service_->root()->GetFilePath();

  // Kick off directory refresh when this setting changes.
  FOR_EACH_OBSERVER(GDataFileSystemInterface::Observer, observers_,
                    OnDirectoryChanged(root_path));
}

//============= GDataFileSystem: internal helper functions =====================

void GDataFileSystem::InitializePreferenceObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_registrar_.reset(new PrefChangeRegistrar());
  pref_registrar_->Init(profile_->GetPrefs());
  pref_registrar_->Add(prefs::kDisableGDataHostedFiles, this);
}

void GDataFileSystem::OpenFile(const FilePath& file_path,
                               const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::OpenFileOnUIThread,
                               ui_weak_ptr_,
                               file_path,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::OpenFileOnUIThread(const FilePath& file_path,
                                         const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the file is already opened, it cannot be opened again before closed.
  // This is for avoiding simultaneous modification to the file, and moreover
  // to avoid an inconsistent cache state (suppose an operation sequence like
  // Open->Open->modify->Close->modify->Close; the second modify may not be
  // synchronized to the server since it is already Closed on the cache).
  if (open_files_.find(file_path) != open_files_.end()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, GDATA_FILE_ERROR_IN_USE, FilePath()));
    return;
  }
  open_files_.insert(file_path);

  directory_service_->GetEntryInfoByPath(
      file_path,
      base::Bind(&GDataFileSystem::OnGetEntryInfoCompleteForOpenFile,
                 ui_weak_ptr_,
                 file_path,
                 base::Bind(&GDataFileSystem::OnOpenFileFinished,
                            ui_weak_ptr_,
                            file_path,
                            callback)));
}

void GDataFileSystem::OnGetEntryInfoCompleteForOpenFile(
    const FilePath& file_path,
    const OpenFileCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = GDATA_FILE_ERROR_NOT_FOUND;

  if (error == GDATA_FILE_OK) {
    if (entry_proto->file_specific_info().file_md5().empty() ||
        entry_proto->file_specific_info().is_hosted_document()) {
      // No support for opening a directory or hosted document.
      error = GDATA_FILE_ERROR_INVALID_OPERATION;
    }
  }

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, FilePath());
    return;
  }

  DCHECK(!entry_proto->resource_id().empty());
  GetResolvedFileByPath(
      file_path,
      base::Bind(&GDataFileSystem::OnGetFileCompleteForOpenFile,
                 ui_weak_ptr_,
                 callback,
                 GetFileCompleteForOpenParams(
                     entry_proto->resource_id(),
                     entry_proto->file_specific_info().file_md5())),
      GetDownloadDataCallback(),
      error,
      entry_proto.get());
}

void GDataFileSystem::OnGetFileCompleteForOpenFile(
    const OpenFileCallback& callback,
    const GetFileCompleteForOpenParams& entry_proto,
    GDataFileError error,
    const FilePath& file_path,
    const std::string& mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, FilePath());
    return;
  }

  // OpenFileOnUIThread ensures that the file is a regular file.
  DCHECK_EQ(REGULAR_FILE, file_type);

  cache_->MarkDirtyOnUIThread(
      entry_proto.resource_id,
      entry_proto.md5,
      base::Bind(&GDataFileSystem::OnMarkDirtyInCacheCompleteForOpenFile,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnMarkDirtyInCacheCompleteForOpenFile(
    const OpenFileCallback& callback,
    GDataFileError error,
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!callback.is_null())
    callback.Run(error, cache_file_path);
}

void GDataFileSystem::OnOpenFileFinished(const FilePath& file_path,
                                         const OpenFileCallback& callback,
                                         GDataFileError result,
                                         const FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // All the invocation of |callback| from operations initiated from OpenFile
  // must go through here. Removes the |file_path| from the remembered set when
  // the file was not successfully opened.
  if (result != GDATA_FILE_OK)
    open_files_.erase(file_path);

  if (!callback.is_null())
    callback.Run(result, cache_file_path);
}

void GDataFileSystem::CloseFile(const FilePath& file_path,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunTaskOnUIThread(base::Bind(&GDataFileSystem::CloseFileOnUIThread,
                               ui_weak_ptr_,
                               file_path,
                               CreateRelayCallback(callback)));
}

void GDataFileSystem::CloseFileOnUIThread(
    const FilePath& file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (open_files_.find(file_path) == open_files_.end()) {
    // The file is not being opened.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, GDATA_FILE_ERROR_NOT_FOUND));
    return;
  }

  // Step 1 of CloseFile: Get resource_id and md5 for |file_path|.
  GetEntryInfoByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnGetEntryInfoCompleteForCloseFile,
                 ui_weak_ptr_,
                 file_path,
                 base::Bind(&GDataFileSystem::OnCloseFileFinished,
                            ui_weak_ptr_,
                            file_path,
                            callback)));
}

void GDataFileSystem::OnGetEntryInfoCompleteForCloseFile(
    const FilePath& file_path,
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = GDATA_FILE_ERROR_NOT_FOUND;

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error);
    return;
  }

  // Step 2 of CloseFile: Get the local path of the cache. Since CloseFile must
  // always be called on paths opened by OpenFile, the file must be cached,
  cache_->GetFileOnUIThread(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5(),
      base::Bind(&GDataFileSystem::OnGetCacheFilePathCompleteForCloseFile,
                 ui_weak_ptr_,
                 file_path,
                 callback));
}

void GDataFileSystem::OnGetCacheFilePathCompleteForCloseFile(
    const FilePath& file_path,
    const FileOperationCallback& callback,
    GDataFileError error,
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error);
    return;
  }

  // Step 3 of CloseFile: Retrieves the (possibly modified) PlatformFileInfo of
  // the cache file.
  base::PlatformFileInfo* file_info = new base::PlatformFileInfo;
  bool* get_file_info_result = new bool(false);
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GetFileInfoOnBlockingPool,
                 local_cache_path,
                 base::Unretained(file_info),
                 base::Unretained(get_file_info_result)),
      base::Bind(&GDataFileSystem::OnGetModifiedFileInfoCompleteForCloseFile,
                 ui_weak_ptr_,
                 file_path,
                 base::Owned(file_info),
                 base::Owned(get_file_info_result),
                 callback));
}

void GDataFileSystem::OnGetModifiedFileInfoCompleteForCloseFile(
    const FilePath& file_path,
    base::PlatformFileInfo* file_info,
    bool* get_file_info_result,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!*get_file_info_result) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND);
    return;
  }

  // Step 4 of CloseFile: Find GDataEntry corresponding to |file_path|, for
  // modifying the entry's metadata.
  FindEntryByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnGetEntryCompleteForCloseFile,
                 ui_weak_ptr_,
                 file_path,
                 *file_info,
                 callback));
}

void GDataFileSystem::OnGetEntryCompleteForCloseFile(
    const FilePath& file_path,
    const base::PlatformFileInfo& file_info,
    const FileOperationCallback& callback,
    GDataFileError error,
    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error);
    return;
  }

  DCHECK(entry);
  GDataFile* file = entry->AsGDataFile();
  if (!file || file->file_md5().empty() || file->is_hosted_document()) {
    // No support for opening a directory or hosted document.
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_INVALID_OPERATION);
    return;
  }
  DCHECK(!file->resource_id().empty());

  // Step 5 of CloseFile:
  // Update the in-memory meta data. Until the committed cache is uploaded in
  // background to the server and the change is propagated back, this in-memory
  // meta data is referred by subsequent file operations. So it needs to reflect
  // the modification made before committing.
  file->set_file_info(file_info);

  // Step 6 of CloseFile: Commit the modification in cache. This will trigger
  // background upload.
  // TODO(benchan,kinaba): Call ClearDirtyInCache instead of CommitDirtyInCache
  // if the file has not been modified. Come up with a way to detect the
  // intactness effectively, or provide a method for user to declare it when
  // calling CloseFile().
  cache_->CommitDirtyOnUIThread(
      file->resource_id(),
      file->file_md5(),
      base::Bind(&GDataFileSystem::OnCommitDirtyInCacheCompleteForCloseFile,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnCommitDirtyInCacheCompleteForCloseFile(
    const FileOperationCallback& callback,
    GDataFileError error,
    const std::string& resource_id,
    const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!callback.is_null())
    callback.Run(error);
}

void GDataFileSystem::OnCloseFileFinished(
    const FilePath& file_path,
    const FileOperationCallback& callback,
    GDataFileError result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Step 7 of CloseFile.
  // All the invocation of |callback| from operations initiated from CloseFile
  // must go through here. Removes the |file_path| from the remembered set so
  // that subsequent operations can open the file again.
  open_files_.erase(file_path);

  // Then invokes the user-supplied callback function.
  if (!callback.is_null())
    callback.Run(result);
}

void GDataFileSystem::CheckLocalModificationAndRun(
    scoped_ptr<GDataEntryProto> entry_proto,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry_proto.get());

  // For entries that will never be cached, use the original entry info as is.
  if (!entry_proto->has_file_specific_info() ||
      entry_proto->file_specific_info().is_hosted_document()) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK, entry_proto.Pass());
    return;
  }

  // Checks if the file is cached and modified locally.
  const std::string resource_id = entry_proto->resource_id();
  const std::string md5 = entry_proto->file_specific_info().file_md5();
  cache_->GetCacheEntryOnUIThread(
      resource_id,
      md5,
      base::Bind(
          &GDataFileSystem::CheckLocalModificationAndRunAfterGetCacheEntry,
          ui_weak_ptr_, base::Passed(&entry_proto), callback));
}

void GDataFileSystem::CheckLocalModificationAndRunAfterGetCacheEntry(
    scoped_ptr<GDataEntryProto> entry_proto,
    const GetEntryInfoCallback& callback,
    bool success,
    const GDataCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // When no dirty cache is found, use the original entry info as is.
  if (!success || !cache_entry.is_dirty()) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK, entry_proto.Pass());
    return;
  }

  // Gets the cache file path.
  const std::string& resource_id = entry_proto->resource_id();
  const std::string& md5 = entry_proto->file_specific_info().file_md5();
  cache_->GetFileOnUIThread(
      resource_id,
      md5,
      base::Bind(
          &GDataFileSystem::CheckLocalModificationAndRunAfterGetCacheFile,
          ui_weak_ptr_, base::Passed(&entry_proto), callback));
}

void GDataFileSystem::CheckLocalModificationAndRunAfterGetCacheFile(
    scoped_ptr<GDataEntryProto> entry_proto,
    const GetEntryInfoCallback& callback,
    GDataFileError error,
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // When no dirty cache is found, use the original entry info as is.
  if (error != GDATA_FILE_OK) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK, entry_proto.Pass());
    return;
  }

  // If the cache is dirty, obtain the file info from the cache file itself.
  base::PlatformFileInfo* file_info = new base::PlatformFileInfo;
  bool* get_file_info_result = new bool(false);
  util::PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&GetFileInfoOnBlockingPool,
                 local_cache_path,
                 base::Unretained(file_info),
                 base::Unretained(get_file_info_result)),
      base::Bind(&GDataFileSystem::CheckLocalModificationAndRunAfterGetFileInfo,
                 ui_weak_ptr_,
                 base::Passed(&entry_proto),
                 callback,
                 base::Owned(file_info),
                 base::Owned(get_file_info_result)));
}

void GDataFileSystem::CheckLocalModificationAndRunAfterGetFileInfo(
    scoped_ptr<GDataEntryProto> entry_proto,
    const GetEntryInfoCallback& callback,
    base::PlatformFileInfo* file_info,
    bool* get_file_info_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!*get_file_info_result) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND, scoped_ptr<GDataEntryProto>());
    return;
  }

  PlatformFileInfoProto entry_file_info;
  GDataEntry::ConvertPlatformFileInfoToProto(*file_info, &entry_file_info);
  *entry_proto->mutable_file_info() = entry_file_info;
  if (!callback.is_null())
    callback.Run(GDATA_FILE_OK, entry_proto.Pass());
}

}  // namespace gdata
