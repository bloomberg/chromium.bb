// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include <errno.h>
#include <sys/stat.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_protocol_handler.h"
#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_filter.h"

using content::BrowserThread;

namespace gdata {
namespace {

const char kMimeTypeJson[] = "application/json";
const char kMimeTypeOctetStream[] = "application/octet-stream";

const char kWildCard[] = "*";
const char kLocallyModifiedFileExtension[] = "local";
const char kMountedArchiveFileExtension[] = "mounted";

const FilePath::CharType kGDataCacheVersionDir[] = FILE_PATH_LITERAL("v1");
const FilePath::CharType kGDataCacheMetaDir[] = FILE_PATH_LITERAL("meta");
const FilePath::CharType kGDataCachePinnedDir[] = FILE_PATH_LITERAL("pinned");
const FilePath::CharType kGDataCacheOutgoingDir[] =
    FILE_PATH_LITERAL("outgoing");
const FilePath::CharType kGDataCachePersistentDir[] =
    FILE_PATH_LITERAL("persistent");
const FilePath::CharType kGDataCacheTmpDir[] = FILE_PATH_LITERAL("tmp");
const FilePath::CharType kGDataCacheTmpDownloadsDir[] =
    FILE_PATH_LITERAL("tmp/downloads");
const FilePath::CharType kGDataCacheTmpDocumentsDir[] =
    FILE_PATH_LITERAL("tmp/documents");
const FilePath::CharType kAccountMetadataFile[] =
    FILE_PATH_LITERAL("account_metadata.json");
const FilePath::CharType kFilesystemProtoFile[] =
    FILE_PATH_LITERAL("file_system.pb");
const FilePath::CharType kSymLinkToDevNull[] = FILE_PATH_LITERAL("/dev/null");

// GData update check interval (in seconds).
#ifndef NDEBUG
const int kGDataUpdateCheckIntervalInSec = 5;
#else
const int kGDataUpdateCheckIntervalInSec = 60;
#endif

// Schedule for dumping root file system proto buffers to disk depending its
// total protobuffer size in MB.
typedef struct {
  double size;
  int timeout;
} SerializationTimetable;

SerializationTimetable kSerializeTimetable[] = {
#ifndef NDEBUG
    {0.5, 0},    // Less than 0.5MB, dump immediately.
    {-1,  1},    // Any size, dump if older than 1 minue.
#else
    {0.5, 0},    // Less than 0.5MB, dump immediately.
    {1.0, 15},   // Less than 1.0MB, dump after 15 minutes.
    {2.0, 30},
    {4.0, 60},
    {-1,  120},  // Any size, dump if older than 120 minues.
#endif
};

// Defines set of parameters sent to callback OnProtoLoaded().
struct LoadRootFeedParams {
  LoadRootFeedParams(
        FilePath search_file_path,
        bool should_load_from_server,
        const FindEntryCallback& callback)
        : search_file_path(search_file_path),
          should_load_from_server(should_load_from_server),
          load_error(base::PLATFORM_FILE_OK),
          callback(callback) {
    }
  ~LoadRootFeedParams() {
  }

  FilePath search_file_path;
  bool should_load_from_server;
  std::string proto;
  base::PlatformFileError load_error;
  base::Time last_modified;
  const FindEntryCallback callback;
};

// Returns true if file system is due to be serialized on disk based on it
// |serialized_size| and |last_serialized| timestamp.
bool ShouldSerializeFileSystemNow(size_t serialized_size,
                                  const base::Time& last_serialized) {
  const double size_in_mb = serialized_size / 1048576.0;
  const int last_proto_dump_in_min =
      (base::Time::Now() - last_serialized).InMinutes();
  for (size_t i = 0; i < arraysize(kSerializeTimetable); i++) {
    if ((size_in_mb < kSerializeTimetable[i].size ||
         kSerializeTimetable[i].size == -1) &&
        last_proto_dump_in_min >= kSerializeTimetable[i].timeout) {
      return true;
    }
  }
  return false;
}

// Returns the home directory path, or an empty string if the home directory
// is not found.
// Copied from webkit/chromeos/cros_mount_point_provider.h.
// TODO(satorux): Share the code.
std::string GetHomeDirectory() {
  if (base::chromeos::IsRunningOnChromeOS())
    return "/home/chronos/user";

  const char* home = getenv("HOME");
  if (home)
    return home;
  return "";
}

// Used to tweak GetAmountOfFreeDiskSpace() behavior for testing.
FreeDiskSpaceGetterInterface* global_free_disk_getter_for_testing = NULL;

// Gets the amount of free disk space. Use
// |global_free_disk_getter_for_testing| if set.
int64 GetAmountOfFreeDiskSpace() {
  if (global_free_disk_getter_for_testing)
    return global_free_disk_getter_for_testing->AmountOfFreeDiskSpace();

  return base::SysInfo::AmountOfFreeDiskSpace(
      FilePath::FromUTF8Unsafe(GetHomeDirectory()));
}

// Returns true if we have sufficient space to store the given number of
// bytes, while keeping kMinFreeSpace bytes on the disk.
bool HasEnoughSpaceFor(int64 num_bytes) {
  int64 free_space = GetAmountOfFreeDiskSpace();
  // Substract this as if this portion does not exist.
  free_space -= kMinFreeSpace;
  return (free_space >= num_bytes);
}

// Remove all files under the given directory, non-recursively.
// Do not remove recursively as we don't want to touch <gache>/tmp/downloads,
// which is used for user initiated downloads like "Save As"
void RemoveAllFiles(const FilePath& directory) {
  using file_util::FileEnumerator;

  FileEnumerator enumerator(directory, false /* recursive */,
                            FileEnumerator::FILES);
  for (FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    DVLOG(1) << "Removing " << file_path.value();
    if (!file_util::Delete(file_path, false /* recursive */))
      LOG(WARNING) << "Failed to delete " << file_path.value();
  }
}

// Converts gdata error code into file platform error code.
base::PlatformFileError GDataToPlatformError(GDataErrorCode status) {
  switch (status) {
    case HTTP_SUCCESS:
    case HTTP_CREATED:
      return base::PLATFORM_FILE_OK;
    case HTTP_UNAUTHORIZED:
    case HTTP_FORBIDDEN:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    case HTTP_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    case GDATA_PARSE_ERROR:
    case GDATA_FILE_ERROR:
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
      PLOG(ERROR) << "Error creating directory " << paths_to_create[i].value();
    } else {
      DVLOG(1) << "Created directory " << paths_to_create[i].value();
    }
  }

  return error;
}

// Changes the permissions of |file_path| to |permissions|.
// Returns the platform error code of the operation.
base::PlatformFileError ChangeFilePermissions(const FilePath& file_path,
                                              mode_t permissions) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;

  if (HANDLE_EINTR(chmod(file_path.value().c_str(), permissions)) != 0) {
    error = SystemToPlatformError(errno);
    PLOG(ERROR) << "Error changing permissions of " << file_path.value();
  } else {
    DVLOG(1) << "Changed permissions of " << file_path.value();
  }

  return error;
}

// Modifies cache state of file on IO thread pool, which involves:
// - moving or copying file (per |file_operation_type|) from |source_path| to
//  |dest_path| if they're different
// - deleting symlink if |symlink_path| is not empty
// - creating symlink if |symlink_path| is not empty and |create_symlink| is
//   true.
base::PlatformFileError ModifyCacheState(
    const FilePath& source_path,
    const FilePath& dest_path,
    GDataFileSystem::FileOperationType file_operation_type,
    const FilePath& symlink_path,
    bool create_symlink) {
  // Move or copy |source_path| to |dest_path| if they are different.
  if (source_path != dest_path) {
    bool success = false;
    if (file_operation_type == GDataFileSystem::FILE_OPERATION_MOVE)
      success = file_util::Move(source_path, dest_path);
    else if (file_operation_type ==
        GDataFileSystem::FILE_OPERATION_COPY)
      success = file_util::CopyFile(source_path, dest_path);
    if (!success) {
      base::PlatformFileError error = SystemToPlatformError(errno);
      PLOG(ERROR) << "Error "
                  << (file_operation_type ==
                      GDataFileSystem::FILE_OPERATION_MOVE ?
                      "moving " : "copying ")
                  << source_path.value()
                  << " to " << dest_path.value();
      return error;
    } else {
      DVLOG(1) << (file_operation_type ==
                   GDataFileSystem::FILE_OPERATION_MOVE ?
                   "Moved " : "Copied ")
               << source_path.value()
               << " to " << dest_path.value();
    }
  } else {
    DVLOG(1) << "No need to move file: source = destination";
  }

  if (symlink_path.empty())
    return base::PLATFORM_FILE_OK;

  // Remove symlink regardless of |create_symlink| because creating a link will
  // not overwrite an existing one.

  // Cannot use file_util::Delete which uses stat64 to check if path exists
  // before deleting it.  If path is a symlink, stat64 dereferences it to the
  // target file, so it's in essence checking if the target file exists.
  // Here in this function, if |symlink_path| references |source_path| and
  // |source_path| has just been moved to |dest_path| (e.g. during unpinning),
  // symlink will dereference to a non-existent file.  This results in stat64
  // failing and file_util::Delete bailing out without deleting the symlink.
  // We clearly want the symlink deleted even if it dereferences to nothing.
  // Unfortunately, deleting the symlink before moving the files won't work for
  // the case where move operation fails, but the symlink has already been
  // deleted, which shouldn't happen.  An example scenario is where an existing
  // file is stored to cache and pinned for a specific resource id and md5, then
  // a non-existent file is stored to cache for the same resource id and md5.
  // The 2nd store-to-cache operation fails when moving files, but the symlink
  // created by previous pin operation has already been deleted.
  // We definitely want to keep the pinned state of the symlink if subsequent
  // operations fail.
  // This problem is filed at http://crbug.com/119430.

  // We try to save one file operation by not checking if link exists before
  // deleting it, so unlink may return error if link doesn't exist, but it
  // doesn't really matter to us.
  bool deleted = HANDLE_EINTR(unlink(symlink_path.value().c_str())) == 0;
  if (deleted) {
    DVLOG(1) << "Deleted symlink " << symlink_path.value();
  } else {
    // Since we didn't check if symlink exists before deleting it, don't log
    // if symlink doesn't exist.
    if (errno != ENOENT)
      PLOG(WARNING) << "Error deleting symlink " << symlink_path.value();
  }

  if (!create_symlink)
    return base::PLATFORM_FILE_OK;

  // Create new symlink to |dest_path|.
  if (!file_util::CreateSymbolicLink(dest_path, symlink_path)) {
    base::PlatformFileError error = SystemToPlatformError(errno);
    PLOG(ERROR) << "Error creating symlink " << symlink_path.value()
                << " for " << dest_path.value();
    return error;
  } else {
    DVLOG(1) << "Created symlink " << symlink_path.value()
             << " to " << dest_path.value();
  }

  return base::PLATFORM_FILE_OK;
}

// Deletes all files that match |path_to_delete_pattern| except for
// |path_to_keep| on IO thread pool.
// If |path_to_keep| is empty, all files in |path_to_delete_pattern| are
// deleted.
void DeleteFilesSelectively(const FilePath& path_to_delete_pattern,
                            const FilePath& path_to_keep) {
  // Enumerate all files in directory of |path_to_delete_pattern| that match
  // base name of |path_to_delete_pattern|.
  // If a file is not |path_to_keep|, delete it.
  bool success = true;
  file_util::FileEnumerator enumerator(
      path_to_delete_pattern.DirName(),
      false,  // not recursive
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::SHOW_SYM_LINKS),
      path_to_delete_pattern.BaseName().value());
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // If |path_to_keep| is not empty and same as current, don't delete it.
    if (!path_to_keep.empty() && current == path_to_keep)
      continue;

    success = HANDLE_EINTR(unlink(current.value().c_str())) == 0;
    if (!success)
      DVLOG(1) << "Error deleting " << current.value();
    else
      DVLOG(1) << "Deleted " << current.value();
  }
}

// Recursively extracts the paths set of all sub-directories of |entry|.
void GetChildDirectoryPaths(GDataEntry* entry,
                            std::set<FilePath>* changed_dirs) {
  GDataDirectory* dir = entry->AsGDataDirectory();
  if (!dir)
    return;

  for (GDataDirectoryCollection::const_iterator it =
       dir->child_directories().begin();
       it != dir->child_directories().end(); ++it) {
    GDataDirectory* child_dir = it->second;
    changed_dirs->insert(child_dir->GetFilePath());
    GetChildDirectoryPaths(child_dir, changed_dirs);
  }
}


// Helper function for removing |entry| from |directory|. If |entry| is a
// directory too, it will collect all its children file paths into
// |changed_dirs| as well.
void RemoveEntryFromDirectoryAndCollectChangedDirectories(
    GDataDirectory* directory,
    GDataEntry* entry,
    std::set<FilePath>* changed_dirs) {
  // Get the list of all sub-directory paths, so we can notify their listeners
  // that they are smoked.
  GetChildDirectoryPaths(entry, changed_dirs);
  directory->RemoveEntry(entry);
}

// Helper function for adding new |file| from the feed into |directory|. It
// checks the type of file and updates |changed_dirs| if this file adding
// opertation needs to raise directory notification update. If file is being
// added to |orphaned_entries_dir| such notifications are not raised since
// we ignore such files and don't add them to the file system now.
void AddEntryToDirectoryAndCollectChangedDirectories(
    GDataEntry* entry,
    GDataDirectory* directory,
    GDataRootDirectory* orphaned_entries_dir,
    std::set<FilePath>* changed_dirs) {
  directory->AddEntry(entry);
  if (entry->AsGDataDirectory() && directory != orphaned_entries_dir)
    changed_dirs->insert(entry->GetFilePath());
}

// Invoked upon completion of TransferRegularFile initiated by Copy.
//
// |callback| is run on the thread represented by |relay_proxy|.
void OnTransferRegularFileCompleteForCopy(
    const FileOperationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    base::PlatformFileError error) {
  if (!callback.is_null())
    relay_proxy->PostTask(FROM_HERE, base::Bind(callback, error));
}

// Runs GetFileCallback with pointers dereferenced.
// Used for PostTaskAndReply().
void RunGetFileCallbackHelper(const GetFileCallback& callback,
                              base::PlatformFileError* error,
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
    base::PlatformFileError* error) {
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error);
}

// Ditto for CacheOperationCallback.
void RunCacheOperationCallbackHelper(
    const CacheOperationCallback& callback,
    base::PlatformFileError* error,
    const std::string& resource_id,
    const std::string& md5) {
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);
}

// Ditto for GetFileFromCacheCallback.
void RunGetFileFromCacheCallbackHelper(
    const GetFileFromCacheCallback& callback,
    base::PlatformFileError* error,
    const std::string& resource_id,
    const std::string& md5,
    FilePath* cache_file_path) {
  DCHECK(error);
  DCHECK(cache_file_path);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5, *cache_file_path);
}

// Ditto for SetMountedStateCallback
void RunSetMountedStateCallbackHelper(
    const SetMountedStateCallback& callback,
    base::PlatformFileError* error,
    FilePath* cache_file_path) {
  DCHECK(error);
  DCHECK(cache_file_path);

  if (!callback.is_null())
    callback.Run(*error, *cache_file_path);
}

void RunGetCacheStateCallbackHelper(
    const GetCacheStateCallback& callback,
    base::PlatformFileError* error,
    int* cache_state) {
  DCHECK(error);
  DCHECK(cache_state);

  if (!callback.is_null())
    callback.Run(*error, *cache_state);
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

// Saves the string |serialized_proto| to a file at |path| on a blocking thread.
void SaveProtoOnIOThreadPool(const FilePath& path,
                             scoped_ptr<std::string> serialized_proto) {
  const int file_size = static_cast<int>(serialized_proto->length());
  if (file_util::WriteFile(path, serialized_proto->data(), file_size) !=
      file_size) {
    LOG(WARNING) << "GData proto file can't be stored at "
                 << path.value();
    if (!file_util::Delete(path, true)) {
      LOG(WARNING) << "GData proto file can't be deleted at "
                   << path.value();
    }
  }
}

// Loads the file at |path| into the string |serialized_proto| on a blocking
// thread.
void LoadProtoOnIOThreadPool(const FilePath& path,
                             LoadRootFeedParams* params) {
  base::PlatformFileInfo info;
  if (!file_util::GetFileInfo(path, &info)) {
    params->load_error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }
  params->last_modified = info.last_modified;
  if (!file_util::ReadFileToString(path, &params->proto)) {
    LOG(WARNING) << "Proto file not found at " << path.value();
    params->load_error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  params->load_error = base::PLATFORM_FILE_OK;
}

// Loads json file content content from |file_path| on IO thread pool.
void LoadJsonFileOnIOThreadPool(
    const FilePath& file_path,
    base::PlatformFileError* error,
    base::Value* result) {
  scoped_ptr<base::Value> root_value;
  std::string contents;
  if (!file_util::ReadFileToString(file_path, &contents)) {
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }

  int unused_error_code = -1;
  std::string unused_error_message;
  root_value.reset(base::JSONReader::ReadAndReturnError(
      contents, base::JSON_PARSE_RFC, &unused_error_code,
      &unused_error_message));

  bool has_root = root_value.get();
  if (!has_root)
    LOG(WARNING) << "Cached content read failed for file " << file_path.value();

  if (!has_root) {
    *error = base::PLATFORM_FILE_ERROR_FAILED;
    return;
  }

  base::ListValue* result_list = NULL;
  base::DictionaryValue* result_dict = NULL;
  if (result->GetAsList(&result_list) &&
      root_value->GetType() == Value::TYPE_LIST) {
    *error = base::PLATFORM_FILE_OK;
    result_list->Swap(reinterpret_cast<base::ListValue*>(root_value.get()));
  } else if (result->GetAsDictionary(&result_dict) &&
             root_value->GetType() == Value::TYPE_DICTIONARY) {
    *error = base::PLATFORM_FILE_OK;
    result_dict->Swap(
        reinterpret_cast<base::DictionaryValue*>(root_value.get()));
  } else {
    *error = base::PLATFORM_FILE_ERROR_FAILED;
  }
}

// Saves json file content content in |feed| to |file_pathname| on IO thread
// pool.
void SaveFeedOnIOThreadPool(
    const FilePath& file_path,
    scoped_ptr<base::Value> feed) {
  std::string json;
#ifndef NDEBUG
  base::JSONWriter::WriteWithOptions(feed.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
#else
  base::JSONWriter::Write(feed.get(), &json);
#endif

  int file_size = static_cast<int>(json.length());
  if (file_util::WriteFile(file_path, json.data(), file_size) != file_size) {
    LOG(WARNING) << "GData metadata file can't be stored at "
                 << file_path.value();
    if (!file_util::Delete(file_path, true)) {
      LOG(WARNING) << "GData metadata file can't be deleted at "
                   << file_path.value();
      return;
    }
  }
}

// Reads properties of |local_file| and fills in values of UploadFileInfo.
// TODO(satorux,achuith): We should just get the file size in this function.
// The rest of the work can be done on UI/IO thread.
void CreateUploadFileInfoOnIOThreadPool(
    const FilePath& local_file,
    const FilePath& remote_dest_file,
    base::PlatformFileError* error,
    UploadFileInfo* upload_file_info) {
  DCHECK(error);
  DCHECK(upload_file_info);

  int64 file_size = 0;
  if (!file_util::GetFileSize(local_file, &file_size)) {
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }

  upload_file_info->file_path = local_file;
  upload_file_info->file_size = file_size;
  // Extract the final path from DownloadItem.
  upload_file_info->gdata_path = remote_dest_file;
  // Use the file name as the title.
  upload_file_info->title = remote_dest_file.BaseName().value();
  upload_file_info->content_length = file_size;
  upload_file_info->all_bytes_present = true;
  std::string mime_type;
  if (!net::GetMimeTypeFromExtension(local_file.Extension(),
                                     &upload_file_info->content_type)) {
    upload_file_info->content_type= kMimeTypeOctetStream;
  }

  *error = base::PLATFORM_FILE_OK;
}

// Checks if a local file at |local_file_path| is a JSON file referencing a
// hosted document on IO thread poll, and if so, gets the resource ID of the
// document.
void GetDocumentResourceIdOnIOThreadPool(
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
// and |resource_id| under |document_dir| on IO thread pool.
void CreateDocumentJsonFileOnIOThreadPool(
    const FilePath& document_dir,
    const GURL& edit_url,
    const std::string& resource_id,
    base::PlatformFileError* error,
    FilePath* temp_file_path,
    std::string* mime_type,
    GDataFileType* file_type) {
  DCHECK(error);
  DCHECK(temp_file_path);
  DCHECK(mime_type);
  DCHECK(file_type);

  *error = base::PLATFORM_FILE_ERROR_FAILED;

  if (file_util::CreateTemporaryFileInDir(document_dir, temp_file_path)) {
    std::string document_content = base::StringPrintf(
        "{\"url\": \"%s\", \"resource_id\": \"%s\"}",
        edit_url.spec().c_str(), resource_id.c_str());
    int document_size = static_cast<int>(document_content.size());
    if (file_util::WriteFile(*temp_file_path, document_content.data(),
                             document_size) == document_size) {
      *error = base::PLATFORM_FILE_OK;
    }
  }

  *mime_type = kMimeTypeJson;
  *file_type = HOSTED_DOCUMENT;
  if (*error != base::PLATFORM_FILE_OK)
      temp_file_path->clear();
}

// Tests if we are allowed to create new directory in the provided directory.
bool ShouldCreateDirectory(const FilePath& directory_path) {
  // We allow directory creation for paths that are on gdata file system
  // (GDATA_SEARCH_PATH_INVALID) and paths that reference actual gdata file
  // system path (GDATA_SEARCH_PATH_RESULT_CHILD).
  util::GDataSearchPathType path_type =
      util::GetSearchPathStatus(directory_path);
  return path_type == util::GDATA_SEARCH_PATH_INVALID ||
         path_type == util::GDATA_SEARCH_PATH_RESULT_CHILD;
}

// Copies a file from |src_file_path| to |dest_file_path| on the local
// file system using file_util::CopyFile. |error| is set to
// base::PLATFORM_FILE_OK on success or base::PLATFORM_FILE_ERROR_FAILED
// otherwise.
void CopyLocalFileOnIOThreadPool(
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    base::PlatformFileError* error) {
  DCHECK(error);

  *error = file_util::CopyFile(src_file_path, dest_file_path) ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_FAILED;
}

// Relays the given FindEntryCallback to another thread via |replay_proxy|.
void RelayFindEntryCallback(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                            const FindEntryCallback& callback,
                            base::PlatformFileError error,
                            const FilePath& directory_path,
                            GDataEntry* entry) {
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback, error, directory_path, entry));
}

// Ditto for FileOperationCallback.
void RelayFileOperationCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const FileOperationCallback& callback,
    base::PlatformFileError error) {
  relay_proxy->PostTask(FROM_HERE, base::Bind(callback, error));
}

// Ditto for GetFileCallback.
void RelayGetFileCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const GetFileCallback& callback,
    base::PlatformFileError error,
    const FilePath& file_path,
    const std::string& mime_type,
    GDataFileType file_type) {
  relay_proxy->PostTask(
      FROM_HERE,
      base::Bind(callback, error, file_path, mime_type, file_type));
}

// Ditto for GetDownloadDataCallback.
void RelayGetDownloadDataCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const GetDownloadDataCallback& callback,
    GDataErrorCode error,
    scoped_ptr<std::string> download_data) {
  // Unlike other callbacks, GetDownloadDataCallback is optional, hence it
  // can be null here.
  if (callback.is_null())
    return;
  relay_proxy->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&download_data)));
}

// Ditto for GetCacheStateCallback.
void RelayGetCacheStateCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const GetCacheStateCallback& callback,
    base::PlatformFileError error,
    int cache_state) {
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback, error, cache_state));
}

// Ditto for GetAvailableSpaceCallback.
void RelayGetAvailableSpaceCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const GetAvailableSpaceCallback& callback,
    base::PlatformFileError error,
    int64 bytes_total,
    int64 bytes_used) {
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback, error, bytes_total, bytes_used));
}

// Ditto for SetMountedStateCallback.
void RelaySetMountedStateCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const SetMountedStateCallback& callback,
    base::PlatformFileError error,
    const FilePath& file_path) {
  relay_proxy->PostTask(FROM_HERE,
                        base::Bind(callback, error, file_path));
}

// Ditto for GetEntryInfoCallback.
void RelayGetEntryInfoCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const GetEntryInfoCallback& callback,
    base::PlatformFileError error,
    const FilePath& entry_path,
    scoped_ptr<GDataEntryProto> entry_proto) {
  relay_proxy->PostTask(
      FROM_HERE,
      base::Bind(callback, error, entry_path, base::Passed(&entry_proto)));
}

// Ditto for GetFileInfoCallback.
void RelayGetFileInfoCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const GetFileInfoCallback& callback,
    base::PlatformFileError error,
    scoped_ptr<GDataFileProto> file_proto) {
  relay_proxy->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&file_proto)));
}

// Ditto for ReadDirectoryCallback.
void RelayReadDirectoryCallback(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const ReadDirectoryCallback& callback,
    base::PlatformFileError error,
    scoped_ptr<GDataDirectoryProto> directory_proto) {
  relay_proxy->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&directory_proto)));
}

}  // namespace

// GDataFileProperties struct implementation.

GDataFileProperties::GDataFileProperties() : is_hosted_document(false) {
}

GDataFileProperties::~GDataFileProperties() {
}

// GDataFileSystem::GetDocumentsParams struct implementation.

GDataFileSystem::GetDocumentsParams::GetDocumentsParams(
    int start_changestamp,
    int root_feed_changestamp,
    std::vector<DocumentFeed*>* feed_list,
    bool should_fetch_multiple_feeds,
    const FilePath& search_file_path,
    const std::string& search_query,
    const FindEntryCallback& callback)
    : start_changestamp(start_changestamp),
      root_feed_changestamp(root_feed_changestamp),
      feed_list(feed_list),
      should_fetch_multiple_feeds(should_fetch_multiple_feeds),
      search_file_path(search_file_path),
      search_query(search_query),
      callback(callback) {
}

GDataFileSystem::GetDocumentsParams::~GetDocumentsParams() {
  STLDeleteElements(feed_list.get());
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

//=================== GetFileFromCacheParams implementation ===================

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

// GDataFileSystem class implementatsion.

GDataFileSystem::GDataFileSystem(Profile* profile,
                                 DocumentsServiceInterface* documents_service)
    : profile_(profile),
      documents_service_(documents_service),
      on_io_completed_(new base::WaitableEvent(
          true /* manual reset */, true /* initially signaled */)),
      cache_initialization_started_(false),
      num_pending_tasks_(0),
      update_timer_(true /* retain_user_task */, true /* is_repeating */),
      hide_hosted_docs_(false),
      ui_weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(
          new base::WeakPtrFactory<GDataFileSystem>(this))),
      ui_weak_ptr_(ui_weak_ptr_factory_->GetWeakPtr()),
      sequence_token_(BrowserThread::GetBlockingPool()->GetSequenceToken()) {
  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void GDataFileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile_->GetPath(), &cache_base_path);
  gdata_cache_path_ = cache_base_path.Append(chrome::kGDataCacheDirname);
  gdata_cache_path_ = gdata_cache_path_.Append(kGDataCacheVersionDir);
  SetCachePaths(gdata_cache_path_);

  documents_service_->Initialize(profile_);

  root_.reset(new GDataRootDirectory);

  PrefService* pref_service = profile_->GetPrefs();
  hide_hosted_docs_ = pref_service->GetBoolean(prefs::kDisableGDataHostedFiles);

  InitializePreferenceObserver();
}

void GDataFileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ContentOrigin initial_origin = root_->origin();
  if (initial_origin == FROM_SERVER) {
    base::AutoLock lock(lock_);
    root_->set_origin(REFRESHING);
    ReloadFeedFromServerIfNeeded(initial_origin,
                                 root_->largest_changestamp(),
                                 root_->GetFilePath(),
                                 base::Bind(&GDataFileSystem::OnUpdateChecked,
                                            ui_weak_ptr_,
                                            initial_origin));
  }
}

void GDataFileSystem::OnUpdateChecked(ContentOrigin initial_origin,
                                      base::PlatformFileError error,
                                      const FilePath& /* directory_path */,
                                      GDataEntry* /* entry */) {
  if (error != base::PLATFORM_FILE_OK) {
    base::AutoLock lock(lock_);
    root_->set_origin(initial_origin);
  }
}

bool GDataFileSystem::SetCacheRootPathForTesting(const FilePath& root_path) {
  if (cache_initialization_started_)
    return false;
  cache_paths_.clear();
  SetCachePaths(root_path);
  return true;
}

GDataFileSystem::~GDataFileSystem() {
  // This should be called from UI thread, from GDataSystemService shutdown.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_registrar_.reset(NULL);

  // ui_weak_ptr_factory_ must be deleted on UI thread.
  ui_weak_ptr_factory_.reset();

  {
    // http://crbug.com/125220
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    // We should wait if there is any pending tasks posted to the worker
    // thread pool. on_io_completed_ won't be signaled iff |num_pending_tasks_|
    // is greater that 0.
    // We don't have to lock with |num_pending_tasks_lock_| here, since number
    // of pending tasks can only decrease at this point (Number of pending class
    // can be increased only on UI and IO thread. We are on UI thread, and there
    // will be no more tasks run on IO thread.
    on_io_completed_->Wait();
  }

  // Now that we are sure that there are no more pending tasks bound to this on
  // other threads, we are safe to destroy the data members.

  // Cancel all the in-flight operations.
  // This asynchronously cancels the URL fetch operations.
  documents_service_->CancelAll();
  documents_service_.reset();

  // Lock to let root destroy cache map and resource map.
  base::AutoLock lock(lock_);
  root_.reset(NULL);

  // Let's make sure that num_pending_tasks_lock_ has been released on all
  // other threads.
  base::AutoLock tasks_lock(num_pending_tasks_lock_);
}

void GDataFileSystem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GDataFileSystem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GDataFileSystem::StartUpdates() {
  DCHECK(!update_timer_.IsRunning());
  update_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(
                          kGDataUpdateCheckIntervalInSec),
                      base::Bind(&GDataFileSystem::CheckForUpdates,
                          ui_weak_ptr_));
}

void GDataFileSystem::StopUpdates() {
  DCHECK(update_timer_.IsRunning());
  update_timer_.Stop();
}

void GDataFileSystem::Authenticate(const AuthStatusCallback& callback) {
  // TokenFetcher, used in DocumentsService, must be run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_->Authenticate(callback);
}

void GDataFileSystem::FindEntryByResourceIdSync(
    const std::string& resource_id,
    FindEntryDelegate* delegate) {
  base::AutoLock lock(lock_);  // To access the cache map.

  GDataFile* file = NULL;
  GDataEntry* entry = root_->GetEntryByResourceId(resource_id);
  if (entry)
    file = entry->AsGDataFile();

  if (file) {
    delegate->OnDone(base::PLATFORM_FILE_OK, file->parent()->GetFilePath(),
                     file);
  } else {
    delegate->OnDone(base::PLATFORM_FILE_ERROR_NOT_FOUND, FilePath(), NULL);
  }
}

void GDataFileSystem::FindEntryByPathAsyncOnUIThread(
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);
  if (root_->origin() == INITIALIZING) {
    // If root feed is not initialized but the initilization process has
    // already started, add an observer to execute the remaining task after
    // the end of the initialization.
    AddObserver(new InitialLoadObserver(
        this,
        base::Bind(&GDataFileSystem::FindEntryByPathSyncOnUIThread,
                   ui_weak_ptr_,
                   search_file_path,
                   callback)));
    return;
  } else if (root_->origin() == UNINITIALIZED) {
    // Load root feed from this disk cache. Upon completion, kick off server
    // fetching.
    root_->set_origin(INITIALIZING);
    LoadRootFeedFromCache(true,  // should_load_from_server
                          search_file_path,
                          callback);
    return;
  }

  // Post a task to the same thread, rather than calling it here, as
  // FindEntryByPathAsync() is asynchronous.
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

  base::AutoLock lock(lock_);  // To access root_.
  FindEntryCallbackRelayDelegate delegate(callback);
  root_->FindEntryByPath(search_file_path, &delegate);
}

void GDataFileSystem::ReloadFeedFromServerIfNeeded(
    ContentOrigin initial_origin,
    int local_changestamp,
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // First fetch the latest changestamp to see if there were any new changes
  // there at all.
  documents_service_->GetAccountMetadata(
      base::Bind(&GDataFileSystem::OnGetAccountMetadata,
                 ui_weak_ptr_,
                 initial_origin,
                 local_changestamp,
                 search_file_path,
                 callback));
}

void GDataFileSystem::OnGetAccountMetadata(
    ContentOrigin initial_origin,
    int local_changestamp,
    const FilePath& search_file_path,
    const FindEntryCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> feed_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::PlatformFileError error = GDataToPlatformError(status);
  if (error != base::PLATFORM_FILE_OK) {
    // Get changes starting from the next changestamp from what we have locally.
    LoadFeedFromServer(local_changestamp + 1, 0,
                       true,  /* should_fetch_multiple_feeds */
                       search_file_path,
                       std::string() /* no search query */,
                       callback,
                       base::Bind(&GDataFileSystem::OnFeedFromServerLoaded,
                                  ui_weak_ptr_));
    return;
  }

  scoped_ptr<AccountMetadataFeed> account_metadata;
  if (feed_data.get())
    account_metadata = AccountMetadataFeed::CreateFrom(*feed_data);

  if (!account_metadata.get()) {
    LoadFeedFromServer(local_changestamp + 1, 0,
                       true,  /* should_fetch_multiple_feeds */
                       search_file_path,
                       std::string() /* no search query */,
                       callback,
                       base::Bind(&GDataFileSystem::OnFeedFromServerLoaded,
                                  ui_weak_ptr_));
    return;
  }

  GDataSystemService* service =
      GDataSystemServiceFactory::GetForProfile(profile_);
  service->webapps_registry()->UpdateFromFeed(account_metadata.get());

  bool changes_detected = true;
  if (local_changestamp >= account_metadata->largest_changestamp()) {
    if (local_changestamp > account_metadata->largest_changestamp()) {
      LOG(WARNING) << "Cached client feed is fresher than server, client = "
                   << local_changestamp
                   << ", server = "
                   << account_metadata->largest_changestamp();
    }
    {
      base::AutoLock lock(lock_);
      root_->set_origin(initial_origin);
      root_->set_refresh_time(base::Time::Now());
    }
    changes_detected = false;
  }

  // No changes detected, continue with search as planned.
  if (!changes_detected) {
    if (!callback.is_null())
      FindEntryByPathSyncOnUIThread(search_file_path, callback);

    NotifyInitialLoadFinished();
    return;
  }

  SaveFeed(feed_data.Pass(), FilePath(kAccountMetadataFile));

  // Load changes from the server.
  LoadFeedFromServer(local_changestamp > 0 ? local_changestamp + 1 : 0,
                     account_metadata->largest_changestamp(),
                     true,  /* should_fetch_multiple_feeds */
                     search_file_path,
                     std::string() /* no search query */,
                     callback,
                     base::Bind(&GDataFileSystem::OnFeedFromServerLoaded,
                                ui_weak_ptr_));
}

void GDataFileSystem::LoadFeedFromServer(
    int start_changestamp,
    int root_feed_changestamp,
    bool should_fetch_multiple_feeds,
    const FilePath& search_file_path,
    const std::string& search_query,
    const FindEntryCallback& entry_found_callback,
    const LoadDocumentFeedCallback& feed_load_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // ...then also kick off document feed fetching from the server as well.
  // |feed_list| will contain the list of all collected feed updates that
  // we will receive through calls of DocumentsService::GetDocuments().
  scoped_ptr<std::vector<DocumentFeed*> > feed_list(
      new std::vector<DocumentFeed*>);
  // Kick off document feed fetching here if we don't have complete data
  // to finish this call.
  documents_service_->GetDocuments(
      GURL(),   // root feed start.
      start_changestamp,
      search_query,
      base::Bind(&GDataFileSystem::OnGetDocuments,
                 ui_weak_ptr_,
                 feed_load_callback,
                 base::Owned(new GetDocumentsParams(start_changestamp,
                                                    root_feed_changestamp,
                                                    feed_list.release(),
                                                    should_fetch_multiple_feeds,
                                                    search_file_path,
                                                    search_query,
                                                    entry_found_callback))));
}

void GDataFileSystem::OnFeedFromServerLoaded(GetDocumentsParams* params,
                                             base::PlatformFileError error) {
  if (error != base::PLATFORM_FILE_OK) {
    if (!params->callback.is_null()) {
      params->callback.Run(error, FilePath(),
                           reinterpret_cast<GDataEntry*>(NULL));
    }
    return;
  }

  error = UpdateFromFeed(*params->feed_list,
                         FROM_SERVER,
                         params->start_changestamp,
                         params->root_feed_changestamp);

  if (error != base::PLATFORM_FILE_OK) {
    if (!params->callback.is_null()) {
      params->callback.Run(error, FilePath(),
                           reinterpret_cast<GDataEntry*>(NULL));
    }

    return;
  }

  // Save file system metadata to disk.
  SaveFileSystemAsProto();

  // If we had someone to report this too, then this retrieval was done in a
  // context of search... so continue search.
  if (!params->callback.is_null()) {
    FindEntryByPathSyncOnUIThread(params->search_file_path, params->callback);
  }
}

void GDataFileSystem::TransferFileFromRemoteToLocal(
    const FilePath& remote_src_file_path,
    const FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

  base::AutoLock lock(lock_);
  // Make sure the destination directory exists
  GDataEntry* dest_dir = GetGDataEntryByPath(
      remote_dest_file_path.DirName());
  if (!dest_dir || !dest_dir->AsGDataDirectory()) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    NOTREACHED();
    return;
  }

  std::string* resource_id = new std::string;
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GetDocumentResourceIdOnIOThreadPool,
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

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  UploadFileInfo* upload_file_info = new UploadFileInfo;
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&CreateUploadFileInfoOnIOThreadPool,
                 local_file_path,
                 remote_dest_file_path,
                 error,
                 upload_file_info),
      base::Bind(&GDataFileSystem::StartFileUploadOnUIThread,
                 ui_weak_ptr_,
                 callback,
                 error,
                 upload_file_info));
}

void GDataFileSystem::StartFileUploadOnUIThread(
    const FileOperationCallback& callback,
    base::PlatformFileError* error,
    UploadFileInfo* upload_file_info) {
  // This method needs to run on the UI thread as required by
  // GDataUploader::UploadFile().
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  DCHECK(upload_file_info);

  GDataSystemService* service =
      GDataSystemServiceFactory::GetForProfile(profile_);

  if (*error == base::PLATFORM_FILE_OK) {
    if (!service)
      *error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (*error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      callback.Run(*error);

    return;
  }

  upload_file_info->completion_callback =
      base::Bind(&GDataFileSystem::OnTransferCompleted,
                 ui_weak_ptr_,
                 callback);

  service->uploader()->UploadFile(scoped_ptr<UploadFileInfo>(upload_file_info));
}

void GDataFileSystem::OnTransferCompleted(
    const FileOperationCallback& callback,
    base::PlatformFileError error,
    UploadFileInfo* upload_file_info) {
  DCHECK(upload_file_info);
  if (error == base::PLATFORM_FILE_OK && upload_file_info->entry.get()) {
    AddUploadedFile(upload_file_info->gdata_path.DirName(),
                    upload_file_info->entry.get(),
                    upload_file_info->file_path,
                    FILE_OPERATION_COPY);
  }
  if (!callback.is_null())
    callback.Run(error);

  // In case of error upload_file_info will be deleted by the uploader.
  if (error != base::PLATFORM_FILE_OK)
    return;

  // TODO(achuith): GDataFileSystem should not have to call DeleteUpload.
  GDataSystemService* service =
      GDataSystemServiceFactory::GetForProfile(profile_);
  if (service)
    service->uploader()->DeleteUpload(upload_file_info);
}

void GDataFileSystem::Copy(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::CopyOnUIThread,
                   ui_weak_ptr_,
                   src_file_path,
                   dest_file_path,
                   base::Bind(&RelayFileOperationCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  CopyOnUIThread(src_file_path, dest_file_path, callback);
}

void GDataFileSystem::CopyOnUIThread(const FilePath& original_src_file_path,
                                     const FilePath& original_dest_file_path,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FilePath dest_parent_path = original_dest_file_path.DirName();

  FilePath src_file_path;
  FilePath dest_file_path;

  std::string src_file_resource_id;
  bool src_file_is_hosted_document = false;
  {
    base::AutoLock lock(lock_);
    GDataEntry* src_entry = GetGDataEntryByPath(original_src_file_path);
    GDataEntry* dest_parent = GetGDataEntryByPath(dest_parent_path);
    if (!src_entry || !dest_parent) {
      error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    } else if (!dest_parent->AsGDataDirectory()) {
      error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    } else if (!src_entry->AsGDataFile() || dest_parent->is_detached()) {
      // TODO(benchan): Implement copy for directories. In the interim,
      // we handle recursive directory copy in the file manager.
      error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
    } else {
      src_file_resource_id = src_entry->resource_id();
      src_file_is_hosted_document =
          src_entry->AsGDataFile()->is_hosted_document();
      // |original_src_file_path| and |original_dest_file_path| don't have to
      // necessary be equal to |src_entry|'s or |dest_entry|'s file path (e.g.
      // paths used to display gdata content search results).
      // That's why, instead of using |original_src_file_path| and
      // |original_dest_file_path|, we will get file paths to use in copy
      // operation from the entries.
      src_file_path = src_entry->GetFilePath();
      dest_parent_path = dest_parent->GetFilePath();
      dest_file_path = dest_parent_path.Append(
          original_dest_file_path.BaseName());
    }
  }

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, error));

    return;
  }

  DCHECK(!src_file_path.empty());
  DCHECK(!dest_file_path.empty());

  if (src_file_is_hosted_document) {
    CopyDocumentToDirectory(dest_parent_path,
                            src_file_resource_id,
                            // Drop the document extension, which should not be
                            // in the document title.
                            dest_file_path.BaseName().RemoveExtension().value(),
                            callback);
    return;
  }

  // TODO(benchan): Reimplement this once the server API supports
  // copying of regular files directly on the server side.
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
    base::PlatformFileError error,
    const FilePath& local_file_path,
    const std::string& unused_mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
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
    base::PlatformFileError error,
    const FilePath& local_file_path,
    const std::string& unused_mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error);

    return;
  }

  // GetFileByPath downloads the file from gdata to a local cache, which is then
  // copied to the actual destination path on the local file system using
  // CopyLocalFileOnIOThreadPool.
  base::PlatformFileError* copy_file_error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&CopyLocalFileOnIOThreadPool,
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

  FilePathUpdateCallback add_file_to_directory_callback =
      base::Bind(&GDataFileSystem::AddEntryToDirectory,
                 ui_weak_ptr_,
                 dir_path,
                 callback);

  documents_service_->CopyDocument(resource_id, new_name,
      base::Bind(&GDataFileSystem::OnCopyDocumentCompleted,
                 ui_weak_ptr_,
                 add_file_to_directory_callback));
}

void GDataFileSystem::Rename(const FilePath& file_path,
                             const FilePath::StringType& new_name,
                             const FilePathUpdateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // It is a no-op if the file is renamed to the same name.
  if (file_path.BaseName().value() == new_name) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, base::PLATFORM_FILE_OK, file_path));
    }
    return;
  }

  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  if (!entry) {
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
  if (entry->AsGDataFile() && entry->AsGDataFile()->is_hosted_document()) {
    FilePath new_file(file_name);
    if (new_file.Extension() == entry->AsGDataFile()->document_extension()) {
      file_name = new_file.RemoveExtension().value();
    }
  }

  documents_service_->RenameResource(
      entry->edit_url(),
      file_name,
      base::Bind(&GDataFileSystem::OnRenameResourceCompleted,
                 ui_weak_ptr_,
                 file_path,
                 file_name,
                 callback));
}

void GDataFileSystem::Move(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::MoveOnUIThread,
                   ui_weak_ptr_,
                   src_file_path,
                   dest_file_path,
                   base::Bind(&RelayFileOperationCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  MoveOnUIThread(src_file_path, dest_file_path, callback);
}

void GDataFileSystem::MoveOnUIThread(const FilePath& original_src_file_path,
                                     const FilePath& original_dest_file_path,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FilePath dest_parent_path = original_dest_file_path.DirName();

  FilePath src_file_path;
  FilePath dest_file_path;
  FilePath dest_name = original_dest_file_path.BaseName();

  {
    // This scoped lock needs to be released before calling Rename() below.
    base::AutoLock lock(lock_);
    GDataEntry* src_entry = GetGDataEntryByPath(original_src_file_path);
    GDataEntry* dest_parent = GetGDataEntryByPath(dest_parent_path);
    if (!src_entry || !dest_parent) {
      error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    } else if (!dest_parent->AsGDataDirectory()) {
        error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    } else if (dest_parent->is_detached()) {
      // We allow moving to a directory without file system root only if it's
      // done as part of renaming (i.e. source and destination parent paths are
      // the same).
      if (original_src_file_path.DirName() != dest_parent_path) {
        error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
      } else {
        // If we are indeed renaming, we have to strip resource id from the file
        // name.
        std::string resource_id;
        std::string file_name;
        util::ParseSearchFileName(dest_name.value(), &resource_id, &file_name);
        if (!file_name.empty())
          dest_name = FilePath(file_name);
      }
    }

    if (error != base::PLATFORM_FILE_OK) {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback, error));
      }
      return;
    }
    // |original_src_file_path| and |original_dest_file_path| don't have to
    // necessary be equal to |src_entry|'s or |dest_entry|'s file path (e.g.
    // paths used to display gdata content search results).
    // That's why, instead of using |original_src_file_path| and
    // |original_dest_file_path|, we will get file paths to use in move
    // operation from the entries.
    src_file_path = src_entry->GetFilePath();
    if (!dest_parent->is_detached())
      dest_parent_path = dest_parent->GetFilePath();
    dest_file_path = dest_parent_path.Append(dest_name);
  }

  DCHECK(!src_file_path.empty());
  DCHECK(!dest_file_path.empty());

  // If the file/directory is moved to the same directory, just rename it.
  if (original_src_file_path.DirName() == dest_parent_path) {
    FilePathUpdateCallback final_file_path_update_callback =
        base::Bind(&GDataFileSystem::OnFilePathUpdated,
                   ui_weak_ptr_,
                   callback);

    Rename(original_src_file_path, dest_name.value(),
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
      base::Bind(&GDataFileSystem::AddEntryToDirectory,
                 ui_weak_ptr_,
                 dest_file_path.DirName(),
                 callback);

  FilePathUpdateCallback remove_file_from_directory_callback =
      base::Bind(&GDataFileSystem::RemoveEntryFromDirectory,
                 ui_weak_ptr_,
                 src_file_path.DirName(),
                 add_file_to_directory_callback);

  Rename(src_file_path, dest_file_path.BaseName().value(),
         remove_file_from_directory_callback);
}

void GDataFileSystem::AddEntryToDirectory(
    const FilePath& dir_path,
    const FileOperationCallback& callback,
    base::PlatformFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  GDataEntry* dir_entry = GetGDataEntryByPath(dir_path);
  if (error == base::PLATFORM_FILE_OK) {
    if (!entry || !dir_entry) {
      error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    } else {
      if (!dir_entry->AsGDataDirectory())
        error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    }
  }

  // Returns if there is an error or |dir_path| is the root directory.
  if (error != base::PLATFORM_FILE_OK || dir_entry->AsGDataRootDirectory()) {
    if (!callback.is_null())
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, error));

    return;
  }

  documents_service_->AddResourceToDirectory(
      dir_entry->content_url(),
      entry->edit_url(),
      base::Bind(&GDataFileSystem::OnAddEntryToDirectoryCompleted,
                 ui_weak_ptr_,
                 callback,
                 file_path,
                 dir_path));
}

void GDataFileSystem::RemoveEntryFromDirectory(
    const FilePath& dir_path,
    const FilePathUpdateCallback& callback,
    base::PlatformFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  GDataEntry* dir = GetGDataEntryByPath(dir_path);
  if (error == base::PLATFORM_FILE_OK) {
    if (!entry || !dir) {
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
      entry->edit_url(),
      entry->resource_id(),
      base::Bind(&GDataFileSystem::OnRemoveEntryFromDirectoryCompleted,
                 ui_weak_ptr_,
                 callback,
                 file_path,
                 dir_path));
}

void GDataFileSystem::Remove(const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::RemoveOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   is_recursive,
                   base::Bind(&RelayFileOperationCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  RemoveOnUIThread(file_path, is_recursive, callback);
}

void GDataFileSystem::RemoveOnUIThread(
    const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  if (!entry) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    }
    return;
  }

  documents_service_->DeleteDocument(
      entry->edit_url(),
      base::Bind(&GDataFileSystem::OnRemovedDocument,
                 ui_weak_ptr_,
                 callback,
                 entry->GetFilePath()));
}

void GDataFileSystem::CreateDirectory(
    const FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::CreateDirectoryOnUIThread,
                   ui_weak_ptr_,
                   directory_path,
                   is_exclusive,
                   is_recursive,
                   base::Bind(&RelayFileOperationCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  CreateDirectoryOnUIThread(
      directory_path, is_exclusive, is_recursive, callback);
}

void GDataFileSystem::CreateDirectoryOnUIThread(
    const FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ShouldCreateDirectory(directory_path)) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
    }
    return;
  }

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
                 ui_weak_ptr_,
                 CreateDirectoryParams(
                     first_missing_path,
                     directory_path,
                     is_exclusive,
                     is_recursive,
                     callback)));
}

void GDataFileSystem::GetFileByPath(
    const FilePath& file_path,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::GetFileByPathOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   base::Bind(&RelayGetFileCallback,
                              base::MessageLoopProxy::current(),
                              get_file_callback),
                   base::Bind(&RelayGetDownloadDataCallback,
                              base::MessageLoopProxy::current(),
                              get_download_data_callback)));
    DCHECK(posted);
    return;
  }

  GetFileByPathOnUIThread(file_path, get_file_callback,
                          get_download_data_callback);
}

void GDataFileSystem::GetFileByPathOnUIThread(
    const FilePath& file_path,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileProperties file_properties;
  if (!GetFileInfoByPath(file_path, &file_properties)) {
    if (!get_file_callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(get_file_callback,
                     base::PLATFORM_FILE_ERROR_NOT_FOUND,
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
  if (file_properties.is_hosted_document) {
    InitializeCacheIfNecessary();

    base::PlatformFileError* error =
        new base::PlatformFileError(base::PLATFORM_FILE_OK);
    FilePath* temp_file_path = new FilePath;
    std::string* mime_type = new std::string;
    GDataFileType* file_type = new GDataFileType(REGULAR_FILE);
    PostBlockingPoolSequencedTaskAndReply(
        FROM_HERE,
        base::Bind(&CreateDocumentJsonFileOnIOThreadPool,
                   GetCacheDirectoryPath(
                       GDataRootDirectory::CACHE_TYPE_TMP_DOCUMENTS),
                   file_properties.alternate_url,
                   file_properties.resource_id,
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
  FilePath local_tmp_path = GetCacheFilePath(file_properties.resource_id,
                                             file_properties.file_md5,
                                             GDataRootDirectory::CACHE_TYPE_TMP,
                                             CACHED_FILE_FROM_SERVER);
  GetFileFromCacheByResourceIdAndMd5(
      file_properties.resource_id,
      file_properties.file_md5,
      base::Bind(
          &GDataFileSystem::OnGetFileFromCache,
          ui_weak_ptr_,
          GetFileFromCacheParams(file_path,
                                 local_tmp_path,
                                 file_properties.content_url,
                                 file_properties.resource_id,
                                 file_properties.file_md5,
                                 file_properties.mime_type,
                                 get_file_callback,
                                 get_download_data_callback)));
}

void GDataFileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::GetFileByResourceIdOnUIThread,
                   ui_weak_ptr_,
                   resource_id,
                   base::Bind(&RelayGetFileCallback,
                              base::MessageLoopProxy::current(),
                              get_file_callback),
                   base::Bind(&RelayGetDownloadDataCallback,
                              base::MessageLoopProxy::current(),
                              get_download_data_callback)));
    DCHECK(posted);
    return;
  }

  GetFileByResourceIdOnUIThread(resource_id, get_file_callback,
                                get_download_data_callback);
}

void GDataFileSystem::GetFileByResourceIdOnUIThread(
    const std::string& resource_id,
    const GetFileCallback& get_file_callback,
    const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath file_path;
  {
    base::AutoLock lock(lock_);  // To access the cache map.
    GDataEntry* entry = root_->GetEntryByResourceId(resource_id);
    if (entry) {
      GDataFile* file = entry->AsGDataFile();
      if (file)
        file_path = file->GetFilePath();
    }
  }

  // Report an error immediately if the file for the resource ID is not
  // found.
  if (file_path.empty()) {
    if (!get_file_callback.is_null()) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(get_file_callback,
                     base::PLATFORM_FILE_ERROR_NOT_FOUND,
                     FilePath(),
                     std::string(),
                     REGULAR_FILE));
    }
    return;
  }

  GetFileByPath(file_path, get_file_callback, get_download_data_callback);
}

void GDataFileSystem::OnGetFileFromCache(const GetFileFromCacheParams& params,
                                         base::PlatformFileError error,
                                         const std::string& resource_id,
                                         const std::string& md5,
                                         const FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Have we found the file in cache? If so, return it back to the caller.
  if (error == base::PLATFORM_FILE_OK) {
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
  // Check if we have enough space, based on the expected file size.
  // - if we don't have enough space, try to free up the disk space
  // - if we still don't have enough space, return "no space" error
  // - if we have enough space, start downloading the file from the server
  int64 file_size = 0;
  {
    base::AutoLock lock(lock_);  // To access the root directory.
    GDataEntry* entry = root_->GetEntryByResourceId(resource_id);
    if (entry)
      file_size = entry->file_info().size;
  }

  bool* has_enough_space = new bool(false);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::FreeDiskSpaceIfNeededFor,
                 base::Unretained(this),
                 file_size,
                 has_enough_space),
      base::Bind(&GDataFileSystem::StartDownloadFileIfEnoughSpace,
                 ui_weak_ptr_,
                 params,
                 cache_file_path,
                 base::Owned(has_enough_space)));
}

void GDataFileSystem::FreeDiskSpaceIfNeededFor(int64 num_bytes,
                                               bool* has_enough_space) {
  // Do nothing and return if we have enough space.
  *has_enough_space = HasEnoughSpaceFor(num_bytes);
  if (*has_enough_space)
    return;

  // Otherwise, try to free up the disk space.
  DVLOG(1) << "Freeing up disk space for " << num_bytes;
  base::AutoLock lock(lock_);  // To access the cache map.
  // First remove temporary files from the cache map.
  root_->RemoveTemporaryFilesFromCacheMap();
  // Then remove all files under "tmp" directory.
  RemoveAllFiles(GetCacheDirectoryPath(GDataRootDirectory::CACHE_TYPE_TMP));

  // Check the disk space again.
  *has_enough_space = HasEnoughSpaceFor(num_bytes);
}

void GDataFileSystem::FreeDiskSpaceIfNeeded(bool* has_enough_space) {
  FreeDiskSpaceIfNeededFor(0, has_enough_space);
}

void GDataFileSystem::StartDownloadFileIfEnoughSpace(
    const GetFileFromCacheParams& params,
    const FilePath& cache_file_path,
    bool* has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!*has_enough_space) {
    // If no enough space, return PLATFORM_FILE_ERROR_NO_SPACE.
    if (!params.get_file_callback.is_null()) {
      params.get_file_callback.Run(base::PLATFORM_FILE_ERROR_NO_SPACE,
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
      params.content_url,
      base::Bind(&GDataFileSystem::OnFileDownloaded,
                 ui_weak_ptr_,
                 params),
      params.get_download_data_callback);
}

void GDataFileSystem::SetCachePaths(const FilePath& root_path) {
  DCHECK(cache_paths_.empty() && !cache_initialization_started_);

  // Insert into |cache_paths_| in order defined in enum CacheSubDirectoryType.
  cache_paths_.push_back(root_path.Append(kGDataCacheMetaDir));
  cache_paths_.push_back(root_path.Append(kGDataCachePinnedDir));
  cache_paths_.push_back(root_path.Append(kGDataCacheOutgoingDir));
  cache_paths_.push_back(root_path.Append(kGDataCachePersistentDir));
  cache_paths_.push_back(root_path.Append(kGDataCacheTmpDir));
  cache_paths_.push_back(root_path.Append(kGDataCacheTmpDownloadsDir));
  cache_paths_.push_back(root_path.Append(kGDataCacheTmpDocumentsDir));
}

void GDataFileSystem::InitiateUpload(
    const std::string& file_name,
    const std::string& content_type,
    int64 content_length,
    const FilePath& destination_directory,
    const FilePath& virtual_path,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
      callback);
}

void GDataFileSystem::ResumeUpload(
    const ResumeUploadParams& params,
    const ResumeFileUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_->ResumeUpload(params, callback);
}

void GDataFileSystem::GetEntryInfoByPathAsync(
    const FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::GetEntryInfoByPathAsyncOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   base::Bind(&RelayGetEntryInfoCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  GetEntryInfoByPathAsyncOnUIThread(file_path, callback);
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
                                    base::PlatformFileError error,
                                    const FilePath& directory_path,
                                    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, FilePath(), scoped_ptr<GDataEntryProto>());
    return;
  }
  DCHECK(entry);

  scoped_ptr<GDataEntryProto> entry_proto(new GDataEntryProto);
  entry->ToProto(entry_proto.get());

  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_OK,
                 entry->GetFilePath(),
                 entry_proto.Pass());
}

void GDataFileSystem::GetFileInfoByPathAsync(
    const FilePath& file_path,
    const GetFileInfoCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::GetFileInfoByPathAsyncOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   base::Bind(&RelayGetFileInfoCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  GetFileInfoByPathAsyncOnUIThread(file_path, callback);
}

void GDataFileSystem::GetFileInfoByPathAsyncOnUIThread(
    const FilePath& file_path,
    const GetFileInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FindEntryByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnGetFileInfo,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnGetFileInfo(const GetFileInfoCallback& callback,
                                    base::PlatformFileError error,
                                    const FilePath& directory_path,
                                    GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, scoped_ptr<GDataFileProto>());
    return;
  }
  DCHECK(entry);

  GDataFile* file = entry->AsGDataFile();
  if (!file) {
    if (!callback.is_null())
      callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   scoped_ptr<GDataFileProto>());
    return;
  }

  scoped_ptr<GDataFileProto> file_proto(new GDataFileProto);
  file->ToProto(file_proto.get());

  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_OK, file_proto.Pass());
}

void GDataFileSystem::ReadDirectoryByPathAsync(
    const FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::ReadDirectoryByPathAsyncOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   base::Bind(&RelayReadDirectoryCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  ReadDirectoryByPathAsyncOnUIThread(file_path, callback);
}

void GDataFileSystem::ReadDirectoryByPathAsyncOnUIThread(
    const FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FindEntryByPathAsyncOnUIThread(
      file_path,
      base::Bind(&GDataFileSystem::OnReadDirectory,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::OnReadDirectory(const ReadDirectoryCallback& callback,
                                      base::PlatformFileError error,
                                      const FilePath& directory_path,
                                      GDataEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null())
      callback.Run(error, scoped_ptr<GDataDirectoryProto>());
    return;
  }
  DCHECK(entry);

  GDataDirectory* directory = entry->AsGDataDirectory();
  if (!directory) {
    if (!callback.is_null())
      callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   scoped_ptr<GDataDirectoryProto>());
    return;
  }

  scoped_ptr<GDataDirectoryProto> directory_proto(new GDataDirectoryProto);
  directory->ToProto(directory_proto.get());

  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_OK, directory_proto.Pass());
}

bool GDataFileSystem::GetFileInfoByPath(
    const FilePath& file_path, GDataFileProperties* properties) {
  DCHECK(properties);
  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  if (!entry)
    return false;

  properties->file_info = entry->file_info();
  properties->resource_id = entry->resource_id();

  GDataFile* regular_file = entry->AsGDataFile();
  if (regular_file) {
    properties->file_md5 = regular_file->file_md5();
    properties->mime_type = regular_file->content_mime_type();
    properties->content_url = regular_file->content_url();
    properties->alternate_url = regular_file->alternate_url();
    properties->is_hosted_document = regular_file->is_hosted_document();
  }
  return true;
}

GDataEntry* GDataFileSystem::GetGDataEntryByPath(
    const FilePath& file_path) {
  lock_.AssertAcquired();
  // Find directory element within the cached file system snapshot.
  ReadOnlyFindEntryDelegate find_delegate;
  root_->FindEntryByPath(file_path, &find_delegate);
  return find_delegate.entry();
}

void GDataFileSystem::GetCacheState(const std::string& resource_id,
                                    const std::string& md5,
                                    const GetCacheStateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Always post a task to the UI thread to call GetCacheStateOnUIThread even if
  // GetCacheState is called on the UI thread. This ensures that, regardless of
  // whether GDataFileSystem is locked or not, GDataFileSystem is unlocked when
  // GetCacheStateOnUIThread is called.
  const bool posted = BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GDataFileSystem::GetCacheStateOnUIThread,
                 ui_weak_ptr_,
                 resource_id,
                 md5,
                 base::Bind(&RelayGetCacheStateCallback,
                            base::MessageLoopProxy::current(),
                            callback)));
  DCHECK(posted);
}

void GDataFileSystem::GetCacheStateOnUIThread(
    const std::string& resource_id,
    const std::string& md5,
    const GetCacheStateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  int* cache_state = new int(GDataFile::CACHE_STATE_NONE);

  // GetCacheStateOnIOThreadPool won't do file IO, but post it to the thread
  // pool, as it must be performed after the cache is initialized.
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::GetCacheStateOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 error,
                 cache_state),
      base::Bind(&RunGetCacheStateCallbackHelper,
                 callback,
                 base::Owned(error),
                 base::Owned(cache_state)));
}

void GDataFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::GetAvailableSpaceOnUIThread,
                   ui_weak_ptr_,
                   base::Bind(&RelayGetAvailableSpaceCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  GetAvailableSpaceOnUIThread(callback);
}

void GDataFileSystem::GetAvailableSpaceOnUIThread(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_->GetAccountMetadata(
      base::Bind(&GDataFileSystem::OnGetAvailableSpace,
                 ui_weak_ptr_,
                 callback));
}

void GDataFileSystem::SetPinState(const FilePath& file_path,
                                  bool to_pin,
                                  const FileOperationCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::SetPinStateOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   to_pin,
                   base::Bind(&RelayFileOperationCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  SetPinStateOnUIThread(file_path, to_pin, callback);
}

void GDataFileSystem::SetPinStateOnUIThread(
    const FilePath& file_path,
    bool to_pin,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string resource_id, md5;
  {
    base::AutoLock lock(lock_);
    GDataEntry* entry = GetGDataEntryByPath(file_path);
    GDataFile* file = entry ? entry->AsGDataFile() : NULL;

    if (!file) {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback,
            base::PLATFORM_FILE_ERROR_NOT_FOUND));
      }
      return;
    }
    resource_id = file->resource_id();
    md5 = file->file_md5();
  }

  CacheOperationCallback cache_callback;

  if (!callback.is_null()) {
    cache_callback = base::Bind(&GDataFileSystem::OnSetPinStateCompleted,
                                ui_weak_ptr_,
                                callback);
  }

  if (to_pin)
    Pin(resource_id, md5, cache_callback);
  else
    Unpin(resource_id, md5, cache_callback);
}

void GDataFileSystem::SetMountedState(const FilePath& file_path,
                                      bool to_mount,
                                      const SetMountedStateCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::SetMountedStateOnUIThread,
                   ui_weak_ptr_,
                   file_path,
                   to_mount,
                   base::Bind(&RelaySetMountedStateCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  SetMountedStateOnUIThread(file_path, to_mount, callback);
}


void GDataFileSystem::SetMountedStateOnUIThread(
    const FilePath& file_path,
    bool to_mount,
    const SetMountedStateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  FilePath* cache_file_path = new FilePath;
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::SetMountedStateOnIOThreadPool,
                 base::Unretained(this),
                 file_path,
                 to_mount,
                 error,
                 cache_file_path),
      base::Bind(&RunSetMountedStateCallbackHelper,
                 callback,
                 base::Owned(error),
                 base::Owned(cache_file_path)));
}

void GDataFileSystem::SetMountedStateOnIOThreadPool(
    const FilePath& file_path,
    bool to_mount,
    base::PlatformFileError *error,
    FilePath* cache_file_path) {
  DCHECK(error);
  DCHECK(cache_file_path);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  // Parse file path to obtain resource_id, md5 and extra_extension.
  std::string resource_id;
  std::string md5;
  std::string extra_extension;
  util::ParseCacheFilePath(file_path, &resource_id, &md5, &extra_extension);
  // The extra_extension shall be ".mounted" iff we're unmounting.
  DCHECK(!to_mount == (extra_extension == kMountedArchiveFileExtension));

  // Get cache entry associated with the resource_id and md5
  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(resource_id,
                                                               md5);
  if (!entry) {
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }
  if (to_mount == entry->IsMounted()) {
    *error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
    return;
  }

  // Get the subdir type and path for the unmounted state.
  GDataRootDirectory::CacheSubDirectoryType unmounted_subdir =
      entry->IsPinned() ? GDataRootDirectory::CACHE_TYPE_PERSISTENT :
                          GDataRootDirectory::CACHE_TYPE_TMP;
  FilePath unmounted_path = GetCacheFilePath(resource_id, md5, unmounted_subdir,
                                             CACHED_FILE_FROM_SERVER);

  // Get the subdir type and path for the mounted state.
  GDataRootDirectory::CacheSubDirectoryType mounted_subdir =
      GDataRootDirectory::CACHE_TYPE_PERSISTENT;
  FilePath mounted_path = GetCacheFilePath(resource_id, md5, mounted_subdir,
                                           CACHED_FILE_MOUNTED);

  // Determine the source and destination paths for moving the cache blob.
  FilePath source_path;
  GDataRootDirectory::CacheSubDirectoryType dest_subdir;
  int cache_state = entry->cache_state;
  if (to_mount) {
    source_path = unmounted_path;
    *cache_file_path = mounted_path;
    dest_subdir = mounted_subdir;
    cache_state = GDataFile::SetCacheMounted(cache_state);
  } else {
    source_path = mounted_path;
    *cache_file_path = unmounted_path;
    dest_subdir = unmounted_subdir;
    cache_state = GDataFile::ClearCacheMounted(cache_state);
  }

  // Move cache blob from source path to destination path.
  *error = ModifyCacheState(source_path, *cache_file_path,
                            GDataFileSystem::FILE_OPERATION_MOVE,
                            FilePath(), false);
  if (*error == base::PLATFORM_FILE_OK) {
    // Now that cache operation is complete, update cache map
    root_->UpdateCacheMap(resource_id, md5, dest_subdir, cache_state);
  }
}

void GDataFileSystem::OnSetPinStateCompleted(
    const FileOperationCallback& callback,
    base::PlatformFileError error,
    const std::string& resource_id,
    const std::string& md5) {
  callback.Run(error);
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
      feed = AccountMetadataFeed::CreateFrom(*data);
  if (!feed.get()) {
    callback.Run(base::PLATFORM_FILE_ERROR_FAILED, -1, -1);
    return;
  }

  SaveFeed(data.Pass(), FilePath(kAccountMetadataFile));

  callback.Run(base::PLATFORM_FILE_OK,
               feed->quota_bytes_total(),
               feed->quota_bytes_used());
}

GDataOperationRegistry* GDataFileSystem::GetOperationRegistry() {
  return documents_service_->operation_registry();
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
  error = AddNewDirectory(params.created_directory_path.DirName(),
                          created_entry);

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

void GDataFileSystem::OnSearch(const ReadDirectoryCallback& callback,
                               GetDocumentsParams* params,
                               base::PlatformFileError error) {
  // The search results will be returned using virtual directory.
  // The directory is not really part of the file system, so it has no parent or
  // root.
  scoped_ptr<GDataDirectory> search_dir(new GDataDirectory(NULL, NULL));

  base::AutoLock lock(lock_);

  int delta_feed_changestamp = 0;
  int num_regular_files = 0;
  int num_hosted_documents = 0;
  FileResourceIdMap file_map;
  if (error == base::PLATFORM_FILE_OK) {
    error = FeedToFileResourceMap(*params->feed_list,
                                  &file_map,
                                  &delta_feed_changestamp,
                                  &num_regular_files,
                                  &num_hosted_documents);
  }

  if (error == base::PLATFORM_FILE_OK) {
    std::set<FilePath> ignored;

    // Go through all entires generated by the feed and add them to the search
    // result directory.
    for (FileResourceIdMap::const_iterator it = file_map.begin();
         it != file_map.end(); ++it) {
      scoped_ptr<GDataEntry> entry(it->second);
      DCHECK_EQ(it->first, entry->resource_id());
      DCHECK(!entry->is_deleted());

      entry->set_title(entry->resource_id() + "." + entry->title());

      search_dir->AddEntry(entry.release());
    }
  }

  scoped_ptr<GDataDirectoryProto> directory_proto(new GDataDirectoryProto);
  search_dir->ToProto(directory_proto.get());

  if (!callback.is_null()) {
    callback.Run(error, directory_proto.Pass());
  }
}

void GDataFileSystem::SearchAsync(const std::string& search_query,
                                  const ReadDirectoryCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const bool posted = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::SearchAsyncOnUIThread,
                   ui_weak_ptr_,
                   search_query,
                   base::Bind(&RelayReadDirectoryCallback,
                              base::MessageLoopProxy::current(),
                              callback)));
    DCHECK(posted);
    return;
  }

  SearchAsyncOnUIThread(search_query, callback);
}

void GDataFileSystem::SearchAsyncOnUIThread(
    const std::string& search_query,
    const ReadDirectoryCallback& callback) {
  scoped_ptr<std::vector<DocumentFeed*> > feed_list(
      new std::vector<DocumentFeed*>);

  LoadFeedFromServer(0, 0,  // We don't use change stamps when fetching search
                            // data; we always fetch the whole result feed.
                     false,  // Stop fetching search results after first feed
                             // chunk to avoid displaying huge number of search
                             // results (especially since we don't cache them).
                     FilePath(),  // Not used.
                     search_query,
                     FindEntryCallback(),  // Not used.
                     base::Bind(&GDataFileSystem::OnSearch,
                                ui_weak_ptr_, callback));
}

void GDataFileSystem::OnGetDocuments(const LoadDocumentFeedCallback& callback,
                                     GetDocumentsParams* params,
                                     GDataErrorCode status,
                                     scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::PlatformFileError error = GDataToPlatformError(status);
  if (error == base::PLATFORM_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (error != base::PLATFORM_FILE_OK) {
    if (!callback.is_null()) {
      callback.Run(params, error);
    }

    return;
  }

  // TODO(zelidrag): Find a faster way to get next url rather than parsing
  // the entire feed.
  GURL next_feed_url;
  scoped_ptr<DocumentFeed> current_feed(DocumentFeed::ExtractAndParse(*data));
  if (!current_feed.get()) {
    if (!callback.is_null()) {
      callback.Run(params, base::PLATFORM_FILE_ERROR_FAILED);
    }

    return;
  }
  const bool has_next_feed_url = current_feed->GetNextFeedURL(&next_feed_url);

#ifndef NDEBUG
  // Save initial root feed for analysis.
  std::string file_name =
      base::StringPrintf("DEBUG_feed_%d.json",
                         params->start_changestamp);
  SaveFeed(data.Pass(), FilePath(file_name));
#endif

  // Add the current feed to the list of collected feeds for this directory.
  params->feed_list->push_back(current_feed.release());

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->feed_list->size(); ++i)
    num_accumulated_entries += params->feed_list->at(i)->entries().size();
  NotifyDocumentFeedFetched(num_accumulated_entries);

  // Check if we need to collect more data to complete the directory list.
  if (params->should_fetch_multiple_feeds && has_next_feed_url &&
      !next_feed_url.is_empty()) {
    // Kick of the remaining part of the feeds.
    documents_service_->GetDocuments(
        next_feed_url,
        params->start_changestamp,
        params->search_query,
        base::Bind(&GDataFileSystem::OnGetDocuments,
                   ui_weak_ptr_,
                   callback,
                   base::Owned(
                       new GetDocumentsParams(
                           params->start_changestamp,
                           params->root_feed_changestamp,
                           params->feed_list.release(),
                           params->should_fetch_multiple_feeds,
                           params->search_file_path,
                           params->search_query,
                           params->callback))));
    return;
  }

  if (!callback.is_null())
    callback.Run(params, error);
}

void GDataFileSystem::LoadRootFeedFromCache(
    bool should_load_from_server,
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FilePath path =
      GetCacheDirectoryPath(GDataRootDirectory::CACHE_TYPE_META).Append(
          kFilesystemProtoFile);
  LoadRootFeedParams* params = new LoadRootFeedParams(search_file_path,
                                                      should_load_from_server,
                                                      callback);
  BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
      base::Bind(&LoadProtoOnIOThreadPool, path, params),
      base::Bind(&GDataFileSystem::OnProtoLoaded,
                 ui_weak_ptr_,
                 base::Owned(params)));
}

void GDataFileSystem::OnProtoLoaded(LoadRootFeedParams* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock lock(lock_);
    // If we have already received updates from the server, bail out.
    if (root_->origin() == FROM_SERVER)
      return;
  }
  int local_changestamp = 0;
  // Update directory structure only if everything is OK and we haven't yet
  // received the feed from the server yet.
  if (params->load_error == base::PLATFORM_FILE_OK) {
    DVLOG(1) << "ParseFromString";
    base::AutoLock lock(lock_);  // To access root_.
    if (root_->ParseFromString(params->proto)) {
      root_->set_last_serialized(params->last_modified);
      root_->set_serialized_size(params->proto.size());
      NotifyInitialLoadFinished();
      local_changestamp = root_->largest_changestamp();
    } else {
      params->load_error = base::PLATFORM_FILE_ERROR_FAILED;
      LOG(WARNING) << "Parse of cached proto file failed";
    }
  }

  FindEntryCallback callback = params->callback;
  // If we got feed content from cache, try search over it.
  if (!params->should_load_from_server ||
      (params->load_error == base::PLATFORM_FILE_OK && !callback.is_null())) {
    // Continue file content search operation if the delegate hasn't terminated
    // this search branch already.
    FindEntryByPathSyncOnUIThread(params->search_file_path, callback);
    callback.Reset();
  }

  if (!params->should_load_from_server)
    return;

  {
    base::AutoLock lock(lock_);
    if (root_->origin() != INITIALIZING)
      root_->set_origin(REFRESHING);
  }

  // Kick of the retrieval of the feed from server. If we have previously
  // |reported| to the original callback, then we just need to refresh the
  // content without continuing search upon operation completion.
  ReloadFeedFromServerIfNeeded(FROM_CACHE,
                               local_changestamp,
                               params->search_file_path,
                               callback);
}

void GDataFileSystem::SaveFileSystemAsProto() {
  DVLOG(1) << "SaveFileSystemAsProto";
  base::AutoLock lock(lock_);  // To access root_.

  if (!ShouldSerializeFileSystemNow(root_->serialized_size(),
                                    root_->last_serialized())) {
    return;
  }

  const FilePath path =
      GetCacheDirectoryPath(GDataRootDirectory::CACHE_TYPE_META).Append(
          kFilesystemProtoFile);
  scoped_ptr<std::string> serialized_proto(new std::string());
  root_->SerializeToString(serialized_proto.get());
  root_->set_last_serialized(base::Time::Now());
  root_->set_serialized_size(serialized_proto->size());
  PostBlockingPoolSequencedTask(
      FROM_HERE,
      base::Bind(&SaveProtoOnIOThreadPool, path,
                 base::Passed(serialized_proto.Pass())));
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

  scoped_ptr<DocumentEntry> doc_entry(DocumentEntry::CreateFrom(entry_value));
  if (!doc_entry.get()) {
    if (!callback.is_null())
      callback.Run(base::PLATFORM_FILE_ERROR_FAILED, FilePath());

    return;
  }

  FilePath file_path;
  {
    base::AutoLock lock(lock_);
    GDataEntry* entry =
        GDataEntry::FromDocumentEntry(
            root_.get(), doc_entry.get(), root_.get());
    if (!entry) {
      if (!callback.is_null())
        callback.Run(base::PLATFORM_FILE_ERROR_FAILED, FilePath());

      return;
    }
    root_->AddEntry(entry);
    file_path = entry->GetFilePath();
  }

  NotifyDirectoryChanged(file_path.DirName());

  if (!callback.is_null())
    callback.Run(error, file_path);
}

void GDataFileSystem::OnAddEntryToDirectoryCompleted(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    const FilePath& dir_path,
    GDataErrorCode status,
    const GURL& document_url) {
  base::PlatformFileError error = GDataToPlatformError(status);
  if (error == base::PLATFORM_FILE_OK)
    error = AddEntryToDirectoryOnFilesystem(file_path, dir_path);

  if (!callback.is_null())
    callback.Run(error);
}

void GDataFileSystem::OnRemoveEntryFromDirectoryCompleted(
    const FilePathUpdateCallback& callback,
    const FilePath& file_path,
    const FilePath& dir_path,
    GDataErrorCode status,
    const GURL& document_url) {
  FilePath updated_file_path = file_path;
  base::PlatformFileError error = GDataToPlatformError(status);
  if (error == base::PLATFORM_FILE_OK)
    error = RemoveEntryFromDirectoryOnFilesystem(file_path, dir_path,
                                                 &updated_file_path);

  if (!callback.is_null())
    callback.Run(error, updated_file_path);
}

void GDataFileSystem::SaveFeed(scoped_ptr<base::Value> feed,
                               const FilePath& name) {
  InitializeCacheIfNecessary();

  PostBlockingPoolSequencedTask(
      FROM_HERE,
      base::Bind(&SaveFeedOnIOThreadPool,
                 GetCacheDirectoryPath(
                     GDataRootDirectory::CACHE_TYPE_META).Append(name),
                 base::Passed(&feed)));
}

void GDataFileSystem::OnRemovedDocument(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    GDataErrorCode status,
    const GURL& document_url) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK)
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

  // At this point, the disk can be full or nearly full for several reasons:
  // - The expected file size was incorrect and the file was larger
  // - There was an in-flight download operation and it used up space
  // - The disk became full for some user actions we cannot control
  //   (ex. the user might have downloaded a large file from a regular web site)
  //
  // If we don't have enough space, we return PLATFORM_FILE_ERROR_NO_SPACE,
  // and try to free up space, even if the file was downloaded successfully.
  bool* has_enough_space = new bool(false);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::FreeDiskSpaceIfNeeded,
                 base::Unretained(this),
                 has_enough_space),
      base::Bind(&GDataFileSystem::OnFileDownloadedAndSpaceChecked,
                 ui_weak_ptr_,
                 params,
                 status,
                 content_url,
                 downloaded_file_path,
                 base::Owned(has_enough_space)));
}

void GDataFileSystem::OnFileDownloadedAndSpaceChecked(
    const GetFileFromCacheParams& params,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& downloaded_file_path,
    bool* has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::PlatformFileError error = GDataToPlatformError(status);

  // Make sure that downloaded file is properly stored in cache. We don't have
  // to wait for this operation to finish since the user can already use the
  // downloaded file.
  if (error == base::PLATFORM_FILE_OK) {
    if (*has_enough_space) {
      StoreToCache(params.resource_id,
                   params.md5,
                   downloaded_file_path,
                   FILE_OPERATION_MOVE,
                   base::Bind(&GDataFileSystem::OnDownloadStoredToCache,
                              ui_weak_ptr_));
    } else {
      // If we don't have enough space, remove the downloaded file, and
      // report "no space" error.
      PostBlockingPoolSequencedTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&file_util::Delete),
                     downloaded_file_path,
                     false /* recursive*/));
      error = base::PLATFORM_FILE_ERROR_NO_SPACE;
    }
  }

  if (!params.get_file_callback.is_null()) {
    params.get_file_callback.Run(error,
                                 downloaded_file_path,
                                 params.mime_type,
                                 REGULAR_FILE);
  }
}

void GDataFileSystem::OnDownloadStoredToCache(base::PlatformFileError error,
                                              const std::string& resource_id,
                                              const std::string& md5) {
  // Nothing much to do here for now.
}

base::PlatformFileError GDataFileSystem::RenameFileOnFilesystem(
    const FilePath& file_path,
    const FilePath::StringType& new_name,
    FilePath* updated_file_path) {
  DCHECK(updated_file_path);

  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  if (!entry)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  DCHECK(entry->parent());
  entry->set_title(new_name);
  // After changing the title of the entry, call TakeFile() to remove the
  // entry from its parent directory and then add it back in order to go
  // through the file name de-duplication.
  // TODO(achuith/satorux/zel): This code is fragile. The title has been
  // changed, but not the file_name. TakeEntry removes the child based on the
  // old file_name, and then re-adds the child by first assigning the new title
  // to file_name. http://crbug.com/30157
  if (!entry->parent()->TakeEntry(entry))
    return base::PLATFORM_FILE_ERROR_FAILED;

  *updated_file_path = entry->GetFilePath();

  NotifyDirectoryChanged(updated_file_path->DirName());
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::AddEntryToDirectoryOnFilesystem(
    const FilePath& file_path, const FilePath& dir_path) {
  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  if (!entry)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  DCHECK_EQ(root_.get(), entry->parent());

  GDataEntry* dir_entry = GetGDataEntryByPath(dir_path);
  if (!dir_entry)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  GDataDirectory* dir = dir_entry->AsGDataDirectory();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  if (!dir->TakeEntry(entry))
    return base::PLATFORM_FILE_ERROR_FAILED;

  NotifyDirectoryChanged(dir_path);
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::RemoveEntryFromDirectoryOnFilesystem(
    const FilePath& file_path, const FilePath& dir_path,
    FilePath* updated_file_path) {
  DCHECK(updated_file_path);

  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(file_path);
  if (!entry)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  GDataEntry* dir = GetGDataEntryByPath(dir_path);
  if (!dir)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  if (!dir->AsGDataDirectory())
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  DCHECK_EQ(dir->AsGDataDirectory(), entry->parent());

  if (!root_->TakeEntry(entry))
    return base::PLATFORM_FILE_ERROR_FAILED;

  *updated_file_path = entry->GetFilePath();

  NotifyDirectoryChanged(updated_file_path->DirName());
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::RemoveEntryFromFileSystem(
    const FilePath& file_path) {

  std::string resource_id;
  base::PlatformFileError error = RemoveEntryFromGData(file_path, &resource_id);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  // If resource_id is not empty, remove its corresponding file from cache.
  if (!resource_id.empty())
    RemoveFromCache(resource_id, CacheOperationCallback());

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::UpdateFromFeed(
    const std::vector<DocumentFeed*>& feed_list,
    ContentOrigin origin,
    int start_changestamp,
    int root_feed_changestamp) {
  DVLOG(1) << "Updating directory with a feed";

  bool is_delta_feed = start_changestamp != 0;
  // We need to lock here as well (despite FindEntryByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);
  bool should_notify_initial_load = root_->origin() == INITIALIZING;

  root_->set_origin(origin);
  root_->set_refresh_time(base::Time::Now());

  int delta_feed_changestamp = 0;
  int num_regular_files = 0;
  int num_hosted_documents = 0;
  FileResourceIdMap file_map;
  base::PlatformFileError error =
      FeedToFileResourceMap(feed_list,
                            &file_map,
                            &delta_feed_changestamp,
                            &num_regular_files,
                            &num_hosted_documents);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  ApplyFeedFromFileUrlMap(
      is_delta_feed,
      is_delta_feed ? delta_feed_changestamp : root_feed_changestamp,
      file_map);

  if (should_notify_initial_load)
    NotifyInitialLoadFinished();

  const int num_total_files = num_hosted_documents + num_regular_files;
  UMA_HISTOGRAM_COUNTS("GData.NumberOfRegularFiles", num_regular_files);
  UMA_HISTOGRAM_COUNTS("GData.NumberOfHostedDocuments",
                       num_hosted_documents);
  UMA_HISTOGRAM_COUNTS("GData.NumberOfTotalFiles", num_total_files);
  return base::PLATFORM_FILE_OK;
}

void GDataFileSystem::ApplyFeedFromFileUrlMap(
    bool is_delta_feed,
    int feed_changestamp,
    const FileResourceIdMap& file_map) {
  lock_.AssertAcquired();
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_directory_changed = is_delta_feed;

  std::set<FilePath> changed_dirs;

  if (!is_delta_feed) {  // Full update.
    root_->RemoveChildren();
    changed_dirs.insert(root_->GetFilePath());
  }
  root_->set_largest_changestamp(feed_changestamp);

  scoped_ptr<GDataRootDirectory> orphaned_entries_dir(
      new GDataRootDirectory);
  // Go through all entires generated by the feed and apply them to the local
  // snapshot of the file system.
  for (FileResourceIdMap::const_iterator it = file_map.begin();
       it != file_map.end(); ++it) {
    scoped_ptr<GDataEntry> entry(it->second);
    DCHECK_EQ(it->first, entry->resource_id());
    GDataEntry* old_entry = root_->GetEntryByResourceId(entry->resource_id());
    GDataDirectory* dest_dir = NULL;
    if (entry->is_deleted()) {  // Deleted file/directory.
      DVLOG(1) << "Removing file " << entry->file_name();
      if (!old_entry)
        continue;

      dest_dir = old_entry->parent();
      if (!dest_dir) {
        NOTREACHED();
        continue;
      }
      RemoveEntryFromDirectoryAndCollectChangedDirectories(
          dest_dir, old_entry, &changed_dirs);
    } else if (old_entry) {  // Change or move of existing entry.
      // Please note that entry rename is just a special case of change here
      // since name is just one of the properties that can change.
      DVLOG(1) << "Changed file " << entry->file_name();
      dest_dir = old_entry->parent();
      if (!dest_dir) {
        NOTREACHED();
        continue;
      }
      // Move children files over if we are dealing with directories.
      if (old_entry->AsGDataDirectory() && entry->AsGDataDirectory()) {
        entry->AsGDataDirectory()->TakeOverEntries(
            old_entry->AsGDataDirectory());
      }
      // Remove the old instance of this entry.
      RemoveEntryFromDirectoryAndCollectChangedDirectories(
          dest_dir, old_entry, &changed_dirs);
      // Did we actually move the new file to another directory?
      if (dest_dir->resource_id() != entry->parent_resource_id()) {
        changed_dirs.insert(dest_dir->GetFilePath());
        dest_dir = FindDirectoryForNewEntry(entry.get(),
                                            file_map,
                                            orphaned_entries_dir.get());
      }
      DCHECK(dest_dir);
      AddEntryToDirectoryAndCollectChangedDirectories(
          entry.release(),
          dest_dir,
          orphaned_entries_dir.get(),
          &changed_dirs);
    } else {  // Adding a new file.
      dest_dir = FindDirectoryForNewEntry(entry.get(),
                                          file_map,
                                          orphaned_entries_dir.get());
      DCHECK(dest_dir);
      AddEntryToDirectoryAndCollectChangedDirectories(
          entry.release(),
          dest_dir,
          orphaned_entries_dir.get(),
          &changed_dirs);
    }

    // Record changed directory if this was a delta feed and the parent
    // directory is already properly rooted within its parent.
    if (dest_dir && (dest_dir->parent() || dest_dir == root_.get()) &&
        dest_dir != orphaned_entries_dir.get() && is_delta_feed) {
      changed_dirs.insert(dest_dir->GetFilePath());
    }
  }

  if (should_notify_directory_changed) {
    for (std::set<FilePath>::iterator dir_iter = changed_dirs.begin();
        dir_iter != changed_dirs.end(); ++dir_iter) {
      NotifyDirectoryChanged(*dir_iter);
    }
  }
}

GDataDirectory* GDataFileSystem::FindDirectoryForNewEntry(
    GDataEntry* new_entry,
    const FileResourceIdMap& file_map,
    GDataRootDirectory* orphaned_entries_dir) {
  GDataDirectory* dir = NULL;
  // Added file.
  const std::string& parent_id = new_entry->parent_resource_id();
  if (parent_id.empty()) {
    dir = root_.get();
    DVLOG(1) << "Root parent for " << new_entry->file_name();
  } else {
    GDataEntry* entry = root_->GetEntryByResourceId(parent_id);
    dir = entry ? entry->AsGDataDirectory() : NULL;
    if (!dir) {
      // The parent directory was also added with this set of feeds.
      FileResourceIdMap::const_iterator find_iter =
          file_map.find(parent_id);
      dir = (find_iter != file_map.end() &&
             find_iter->second) ?
                find_iter->second->AsGDataDirectory() : NULL;
      DVLOG(1) << "Found parent for " << new_entry->file_name()
               << " in map " << parent_id;
      if (!dir) {
        DVLOG(1) << "Adding orphan " << new_entry->GetFilePath().value();
        dir = orphaned_entries_dir;
      }
    }
  }
  return dir;
}

base::PlatformFileError GDataFileSystem::FeedToFileResourceMap(
    const std::vector<DocumentFeed*>& feed_list,
    FileResourceIdMap* file_map,
    int* feed_changestamp,
    int* num_regular_files,
    int* num_hosted_documents) {
  lock_.AssertAcquired();

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  *num_regular_files = 0;
  *num_hosted_documents = 0;
  for (size_t i = 0; i < feed_list.size(); ++i) {
    const DocumentFeed* feed = feed_list[i];

    // Get upload url from the root feed. Links for all other collections will
    // be handled in GDatadirectory::FromDocumentEntry();
    if (i == 0) {
      const Link* root_feed_upload_link =
          feed->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
      if (root_feed_upload_link)
        root_->set_upload_url(root_feed_upload_link->href());
      *feed_changestamp = feed->largest_changestamp();
      DCHECK_GE(*feed_changestamp, 0);
    }

    for (ScopedVector<DocumentEntry>::const_iterator iter =
             feed->entries().begin();
         iter != feed->entries().end(); ++iter) {
      DocumentEntry* doc = *iter;
      GDataEntry* entry = GDataEntry::FromDocumentEntry(NULL, doc,
                                                             root_.get());
      // Some document entries don't map into files (i.e. sites).
      if (!entry)
        continue;
      // Count the number of files.
      GDataFile* as_file = entry->AsGDataFile();
      if (as_file) {
        if (as_file->is_hosted_document())
          ++(*num_hosted_documents);
        else
          ++(*num_regular_files);
      }

      FileResourceIdMap::iterator map_entry =
          file_map->find(entry->resource_id());

      // An entry with the same self link may already exist, so we need to
      // release the existing GDataEntry instance before overwriting the
      // entry with another GDataEntry instance.
      if (map_entry != file_map->end()) {
        LOG(WARNING) << "Found duplicate file "
                     << map_entry->second->file_name();

        delete map_entry->second;
        file_map->erase(map_entry);
      }
      file_map->insert(
          std::pair<std::string, GDataEntry*>(entry->resource_id(), entry));
    }
  }

  if (error != base::PLATFORM_FILE_OK) {
    // If the code above fails to parse a feed, any GDataEntry instance
    // added to |file_by_url| is not managed by a GDataDirectory instance,
    // so we need to explicitly release them here.
    STLDeleteValues(file_map);
  }

  return error;
}


void GDataFileSystem::NotifyCacheInitialized() {
  DVLOG(1) << "Cache initialized";
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::NotifyCacheInitialized,
                   ui_weak_ptr_));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Notify the observers that the cache is initialized.
  FOR_EACH_OBSERVER(Observer, observers_, OnCacheInitialized());
}

void GDataFileSystem::NotifyFilePinned(const std::string& resource_id,
                                       const std::string& md5) {
  DVLOG(1) << "File pinned " << resource_id << ": " << md5;
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::NotifyFilePinned,
                   ui_weak_ptr_,
                   resource_id,
                   md5));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Notify the observers that a file is pinned with |resource_id| and |md5|.
  FOR_EACH_OBSERVER(Observer, observers_, OnFilePinned(resource_id, md5));
}

void GDataFileSystem::NotifyFileUnpinned(const std::string& resource_id,
                                         const std::string& md5) {
  DVLOG(1) << "File unpinned " << resource_id << ": " << md5;
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::NotifyFileUnpinned,
                   ui_weak_ptr_,
                   resource_id,
                   md5));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Notify the observers that a file is unpinned with |resource_id| and |md5|.
  FOR_EACH_OBSERVER(Observer, observers_, OnFileUnpinned(resource_id, md5));
}

void GDataFileSystem::NotifyDirectoryChanged(const FilePath& directory_path) {
  DVLOG(1) << "Content changed of " << directory_path.value();
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::NotifyDirectoryChanged,
                   ui_weak_ptr_,
                   directory_path));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Notify the observers that content of |directory_path| has been changed.
  FOR_EACH_OBSERVER(Observer, observers_, OnDirectoryChanged(directory_path));
}

void GDataFileSystem::NotifyInitialLoadFinished() {
  DVLOG(1) << "Initial load finished";
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::NotifyInitialLoadFinished, ui_weak_ptr_));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Notify the observers that root directory has been initialized.
  FOR_EACH_OBSERVER(Observer, observers_, OnInitialLoadFinished());
}

void GDataFileSystem::NotifyDocumentFeedFetched(int num_accumulated_entries) {
  DVLOG(1) << "Document feed fetched: " << num_accumulated_entries;
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataFileSystem::NotifyDocumentFeedFetched,
                   ui_weak_ptr_,
                   num_accumulated_entries));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Notify the observers that a document feed is fetched.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnDocumentFeedFetched(num_accumulated_entries));
}

base::PlatformFileError GDataFileSystem::AddNewDirectory(
    const FilePath& directory_path, base::Value* entry_value) {
  if (!entry_value)
    return base::PLATFORM_FILE_ERROR_FAILED;

  scoped_ptr<DocumentEntry> doc_entry(DocumentEntry::CreateFrom(entry_value));

  if (!doc_entry.get())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We need to lock here as well (despite FindEntryByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find parent directory element within the cached file system snapshot.
  GDataEntry* entry = GetGDataEntryByPath(directory_path);
  if (!entry)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // Check if parent is a directory since in theory since this is a callback
  // something could in the meantime have nuked the parent dir and created a
  // file with the exact same name.
  GDataDirectory* parent_dir = entry->AsGDataDirectory();
  if (!parent_dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

  GDataEntry* new_entry = GDataEntry::FromDocumentEntry(parent_dir,
                                                        doc_entry.get(),
                                                        root_.get());
  if (!new_entry)
    return base::PLATFORM_FILE_ERROR_FAILED;

  parent_dir->AddEntry(new_entry);

  // |directory_path| is not necessary same as |entry->GetFilePath()|. It may be
  // virtual path that references the entry (e.g. path under which content
  // search result is shown).
  // We want to dispatch directory changed with the actual entry's path.
  NotifyDirectoryChanged(entry->GetFilePath());
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
    GDataEntry* entry = GetGDataEntryByPath(current_path);
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

GURL GDataFileSystem::GetUploadUrlForDirectory(
    const FilePath& destination_directory) {
  // Find directory element within the cached file system snapshot.
  base::AutoLock lock(lock_);
  GDataEntry* entry = GetGDataEntryByPath(destination_directory);
  GDataDirectory* dir = entry ? entry->AsGDataDirectory() : NULL;
  return dir ? dir->upload_url() : GURL();
}

base::PlatformFileError GDataFileSystem::RemoveEntryFromGData(
    const FilePath& file_path, std::string* resource_id) {
  resource_id->clear();

  // We need to lock here as well (despite FindEntryByPath lock) since
  // directory instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  GDataEntry* entry = GetGDataEntryByPath(file_path);

  if (!entry)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // You can't remove root element.
  if (!entry->parent())
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

  // If it's a file (only files have resource id), get its resource id so that
  // we can remove it after releasing the auto lock.
  if (entry->AsGDataFile())
    *resource_id = entry->AsGDataFile()->resource_id();

  GDataDirectory* parent_dir = entry->parent();
  if (!parent_dir->RemoveEntry(entry))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  NotifyDirectoryChanged(parent_dir->GetFilePath());
  return base::PLATFORM_FILE_OK;
}

void GDataFileSystem::AddUploadedFile(const FilePath& virtual_dir_path,
                                      DocumentEntry* entry,
                                      const FilePath& file_content_path,
                                      FileOperationType cache_operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!entry) {
    NOTREACHED();
    return;
  }

  std::string resource_id;
  std::string md5;
  {
    base::AutoLock lock(lock_);
    GDataEntry* dir_entry = GetGDataEntryByPath(virtual_dir_path);
    if (!dir_entry)
      return;

    GDataDirectory* parent_dir  = dir_entry->AsGDataDirectory();
    if (!parent_dir)
      return;

    scoped_ptr<GDataEntry> new_entry(
        GDataEntry::FromDocumentEntry(parent_dir, entry, root_.get()));
    if (!new_entry.get())
      return;

    GDataFile* file = new_entry->AsGDataFile();
    DCHECK(file);
    resource_id = file->resource_id();
    md5 = file->file_md5();
    parent_dir->AddEntry(new_entry.release());
  }
  NotifyDirectoryChanged(virtual_dir_path);

  StoreToCache(resource_id, md5, file_content_path, cache_operation,
               CacheOperationCallback());
}

void GDataFileSystem::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
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

bool GDataFileSystem::hide_hosted_documents() {
  base::AutoLock lock(lock_);
  return hide_hosted_docs_;
}

void GDataFileSystem::SetHideHostedDocuments(bool hide) {
  FilePath root_path;
  {
    base::AutoLock lock(lock_);
    if (hide == hide_hosted_docs_)
      return;

    hide_hosted_docs_ = hide;
    root_path = root_->GetFilePath();
  }

  // Kick of directory refresh when this setting changes.
  NotifyDirectoryChanged(root_path);
}

//===================== GDataFileSystem: Cache entry points ====================

bool GDataFileSystem::IsUnderGDataCacheDirectory(const FilePath& path) const {
  return gdata_cache_path_ == path || gdata_cache_path_.IsParent(path);
}

FilePath GDataFileSystem::GetCacheDirectoryPath(
    GDataRootDirectory::CacheSubDirectoryType sub_dir_type) const {
  DCHECK_LE(0, sub_dir_type);
  DCHECK_GT(GDataRootDirectory::NUM_CACHE_TYPES, sub_dir_type);
  return cache_paths_[sub_dir_type];
}

FilePath GDataFileSystem::GetCacheFilePath(
    const std::string& resource_id,
    const std::string& md5,
    GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
    CachedFileOrigin file_origin) const {
  DCHECK(sub_dir_type != GDataRootDirectory::CACHE_TYPE_META);

  // Runs on any thread.
  // Filename is formatted as resource_id.md5, i.e. resource_id is the base
  // name and md5 is the extension.
  std::string base_name = util::EscapeCacheFileName(resource_id);
  if (file_origin == CACHED_FILE_LOCALLY_MODIFIED) {
    DCHECK(sub_dir_type == GDataRootDirectory::CACHE_TYPE_PERSISTENT);
    base_name += FilePath::kExtensionSeparator;
    base_name += kLocallyModifiedFileExtension;
  } else if (!md5.empty()) {
    base_name += FilePath::kExtensionSeparator;
    base_name += util::EscapeCacheFileName(md5);
  }
  // For mounted archives the filename is formatted as resource_id.md5.mounted,
  // i.e. resource_id.md5 is the base name and ".mounted" is the extension
  if (file_origin == CACHED_FILE_MOUNTED) {
     DCHECK(sub_dir_type == GDataRootDirectory::CACHE_TYPE_PERSISTENT);
     base_name += FilePath::kExtensionSeparator;
     base_name += kMountedArchiveFileExtension;
  }
  return GetCacheDirectoryPath(sub_dir_type).Append(base_name);
}

void GDataFileSystem::StoreToCache(const std::string& resource_id,
                                   const std::string& md5,
                                   const FilePath& source_path,
                                   FileOperationType file_operation_type,
                                   const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::StoreToCacheOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 source_path,
                 file_operation_type,
                 error),
      base::Bind(&RunCacheOperationCallbackHelper,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5));
}

void GDataFileSystem::Pin(const std::string& resource_id,
                          const std::string& md5,
                          const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::PinOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 FILE_OPERATION_MOVE,
                 error),
      base::Bind(&GDataFileSystem::OnFilePinned,
                 ui_weak_ptr_,
                 base::Owned(error),
                 resource_id,
                 md5,
                 callback));
}

void GDataFileSystem::Unpin(const std::string& resource_id,
                            const std::string& md5,
                            const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::UnpinOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 FILE_OPERATION_MOVE,
                 error),
      base::Bind(&GDataFileSystem::OnFileUnpinned,
                 ui_weak_ptr_,
                 base::Owned(error),
                 resource_id,
                 md5,
                 callback));
}

void GDataFileSystem::MarkDirtyInCache(
    const std::string& resource_id,
    const std::string& md5,
    const GetFileFromCacheCallback& callback) {
  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  FilePath* cache_file_path = new FilePath;
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::MarkDirtyInCacheOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 FILE_OPERATION_MOVE,
                 error,
                 cache_file_path),
      base::Bind(&RunGetFileFromCacheCallbackHelper,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5,
                 base::Owned(cache_file_path)));
}

void GDataFileSystem::CommitDirtyInCache(
    const std::string& resource_id,
    const std::string& md5,
    const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::CommitDirtyInCacheOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 FILE_OPERATION_MOVE,
                 error),
      base::Bind(&RunCacheOperationCallbackHelper,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5));
}

void GDataFileSystem::ClearDirtyInCache(
    const std::string& resource_id,
    const std::string& md5,
    const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::ClearDirtyInCacheOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 FILE_OPERATION_MOVE,
                 error),
      base::Bind(&RunCacheOperationCallbackHelper,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5));
}

void GDataFileSystem::RemoveFromCache(const std::string& resource_id,
                                      const CacheOperationCallback& callback) {
  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);

  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::RemoveFromCacheOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 error),
      base::Bind(&RunCacheOperationCallbackHelper,
                 callback,
                 base::Owned(error),
                 resource_id,
                 ""  /* md5 */));
}

void GDataFileSystem::InitializeCacheIfNecessary() {
  // Lock to access cache_initialization_started_.
  base::AutoLock lock(lock_);

  // Cache initialization is either in progress or has completed.
  if (cache_initialization_started_)
    return;

  // Need to initialize cache.
  cache_initialization_started_ = true;
  PostBlockingPoolSequencedTask(
      FROM_HERE,
      base::Bind(&GDataFileSystem::InitializeCacheOnIOThreadPool,
                 base::Unretained(this)));
}

//========= GDataFileSystem: Cache tasks that ran on io thread pool ============

void GDataFileSystem::InitializeCacheOnIOThreadPool() {
  base::PlatformFileError error = CreateCacheDirectories(cache_paths_);
  if (error != base::PLATFORM_FILE_OK)
    return;

  // Change permissions of cache persistent directory to u+rwx,og+x in order to
  // allow archive files in that directory to be mounted by cros-disks.
  error = ChangeFilePermissions(
      GetCacheDirectoryPath(GDataRootDirectory::CACHE_TYPE_PERSISTENT),
      S_IRWXU | S_IXGRP | S_IXOTH);
  if (error != base::PLATFORM_FILE_OK)
    return;

  // Scan cache persistent and tmp directories to enumerate all files and create
  // corresponding entries for cache map.
  GDataRootDirectory::CacheMap cache_map;
  ScanCacheDirectory(GDataRootDirectory::CACHE_TYPE_PERSISTENT, &cache_map);
  ScanCacheDirectory(GDataRootDirectory::CACHE_TYPE_TMP, &cache_map);

  // Then scan pinned and outgoing directories to update existing entries in
  // cache map, or create new ones for pinned symlinks to /dev/null which target
  // nothing.
  // Pinned and outgoing directories should be scanned after the persistent
  // directory as we'll add PINNED and DIRTY states respectively to the existing
  // files in the persistent directory per the contents of the pinned and
  // outgoing directories.
  ScanCacheDirectory(GDataRootDirectory::CACHE_TYPE_PINNED, &cache_map);
  ScanCacheDirectory(GDataRootDirectory::CACHE_TYPE_OUTGOING, &cache_map);

  // Lock to update cache map.
  base::AutoLock lock(lock_);
  root_->SetCacheMap(cache_map);

  NotifyCacheInitialized();
}

void GDataFileSystem::GetFileFromCacheOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    base::PlatformFileError* error,
    FilePath* cache_file_path) {
  DCHECK(error);
  DCHECK(cache_file_path);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(resource_id,
                                                               md5);
  if (entry && entry->IsPresent()) {
    CachedFileOrigin file_origin;
    if (entry->IsMounted()) {
      file_origin = CACHED_FILE_MOUNTED;
    } else if (entry->IsDirty()) {
      file_origin = CACHED_FILE_LOCALLY_MODIFIED;
    } else {
      file_origin = CACHED_FILE_FROM_SERVER;
    }
    *cache_file_path = GetCacheFilePath(
        resource_id,
        md5,
        entry->sub_dir_type,
        file_origin);
    *error = base::PLATFORM_FILE_OK;
  } else {
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
}

void GDataFileSystem::GetCacheStateOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    base::PlatformFileError* error,
    int* cache_state) {
  DCHECK(error);
  DCHECK(cache_state);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  *error = base::PLATFORM_FILE_OK;
  *cache_state = GDataFile::CACHE_STATE_NONE;

  // Get file object for |resource_id|.
  GDataEntry* entry = root_->GetEntryByResourceId(resource_id);
  if (!entry || !entry->AsGDataFile()) {
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  } else {
    // Get cache state of file corresponding to |resource_id| and |md5|.
    GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(resource_id,
                                                                 md5);
    if (entry)
      *cache_state = entry->cache_state;
  }
}

void GDataFileSystem::StoreToCacheOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    const FilePath& source_path,
    FileOperationType file_operation_type,
    base::PlatformFileError* error) {
  DCHECK(error);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  FilePath dest_path;
  FilePath symlink_path;
  int cache_state = GDataFile::CACHE_STATE_PRESENT;
  GDataRootDirectory::CacheSubDirectoryType sub_dir_type =
      GDataRootDirectory::CACHE_TYPE_TMP;

  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, md5);

  // If file was previously pinned, store it in persistent dir and create
  // symlink in pinned dir.
  if (entry) {  // File exists in cache.
    // If file is dirty or mounted, return error.
    if (entry->IsDirty() || entry->IsMounted()) {
      LOG(WARNING) << "Can't store a file to replace a "
                   << (entry->IsDirty() ? "dirty" : "mounted")
                   << " file: res_id=" << resource_id
                   << ", md5=" << md5;
      *error = base::PLATFORM_FILE_ERROR_IN_USE;
      return;
    }

    cache_state |= entry->cache_state;

    // If file is pinned, determines destination path.
    if (entry->IsPinned()) {
      sub_dir_type = GDataRootDirectory::CACHE_TYPE_PERSISTENT;
      dest_path = GetCacheFilePath(resource_id, md5, sub_dir_type,
                                   CACHED_FILE_FROM_SERVER);
      symlink_path = GetCacheFilePath(resource_id, std::string(),
                                      GDataRootDirectory::CACHE_TYPE_PINNED,
                                      CACHED_FILE_FROM_SERVER);
    }
  }

  // File wasn't pinned or doesn't exist in cache, store in tmp dir.
  if (dest_path.empty()) {
    DCHECK_EQ(GDataRootDirectory::CACHE_TYPE_TMP, sub_dir_type);
    dest_path = GetCacheFilePath(resource_id, md5, sub_dir_type,
                                 CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(
      source_path,
      dest_path,
      file_operation_type,
      symlink_path,
      !symlink_path.empty());  // create symlink

  // Determine search pattern for stale filenames corrresponding to resource_id,
  // either "<resource_id>*" or "<resource_id>.*".
  FilePath stale_filenames_pattern;
  if (md5.empty()) {
    // No md5 means no extension, append '*' after base name, i.e.
    // "<resource_id>*".
    // Cannot call |dest_path|.ReplaceExtension when there's no md5 extension:
    // if base name of |dest_path| (i.e. escaped resource_id) contains the
    // extension separator '.', ReplaceExtension will remove it and everything
    // after it.  The result will be nothing like the escaped resource_id.
    stale_filenames_pattern = FilePath(dest_path.value() + kWildCard);
  } else {
    // Replace md5 extension with '*' i.e. "<resource_id>.*".
    // Note that ReplaceExtension automatically prefixes the extension with the
    // extension separator '.'.
    stale_filenames_pattern = dest_path.ReplaceExtension(kWildCard);
  }

  // Delete files that match |stale_filenames_pattern| except for |dest_path|.
  DeleteFilesSelectively(stale_filenames_pattern, dest_path);

  if (*error == base::PLATFORM_FILE_OK) {
    // Now that file operations have completed, update cache map.
    root_->UpdateCacheMap(resource_id, md5, sub_dir_type,
                          cache_state);
  }
}

void GDataFileSystem::PinOnIOThreadPool(const std::string& resource_id,
                                        const std::string& md5,
                                        FileOperationType file_operation_type,
                                        base::PlatformFileError* error) {
  DCHECK(error);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  FilePath source_path;
  FilePath dest_path;
  FilePath symlink_path;
  bool create_symlink = true;
  int cache_state = GDataFile::CACHE_STATE_PINNED;
  GDataRootDirectory::CacheSubDirectoryType sub_dir_type =
      GDataRootDirectory::CACHE_TYPE_PERSISTENT;

  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, md5);

  if (!entry) {  // Entry does not exist in cache.
    // Set both |dest_path| and |source_path| to /dev/null, so that:
    // 1) ModifyCacheState won't move files when |source_path| and |dest_path|
    //    are the same.
    // 2) symlinks to /dev/null will be picked up by GDataSyncClient to download
    //    pinned files that don't exist in cache.
    dest_path = FilePath(kSymLinkToDevNull);
    source_path = dest_path;

    // Set sub_dir_type to PINNED to indicate that the file doesn't exist.
    // When the file is finally downloaded and StoreToCache called, it will be
    // moved to persistent directory.
    sub_dir_type = GDataRootDirectory::CACHE_TYPE_PINNED;
  } else {  // File exists in cache, determines destination path.
    cache_state |= entry->cache_state;

    // Determine source and destination paths.

    // If file is dirty or mounted, don't move it, so determine |dest_path| and
    // set |source_path| the same, because ModifyCacheState only moves files if
    // source and destination are different.
    if (entry->IsDirty() || entry->IsMounted()) {
      DCHECK_EQ(GDataRootDirectory::CACHE_TYPE_PERSISTENT, entry->sub_dir_type);
      dest_path = GetCacheFilePath(resource_id,
                                   md5,
                                   entry->sub_dir_type,
                                   CACHED_FILE_LOCALLY_MODIFIED);
      source_path = dest_path;
    } else {
      // Gets the current path of the file in cache.
      source_path = GetCacheFilePath(resource_id,
                                     md5,
                                     entry->sub_dir_type,
                                     CACHED_FILE_FROM_SERVER);

      // If file was pinned before but actual file blob doesn't exist in cache:
      // - don't need to move the file, so set |dest_path| to |source_path|,
      //   because ModifyCacheState only moves files if source and destination
      //   are different
      // - don't create symlink since it already exists.
      if (entry->sub_dir_type == GDataRootDirectory::CACHE_TYPE_PINNED) {
        dest_path = source_path;
        create_symlink = false;
      } else {  // File exists, move it to persistent dir.
        dest_path = GetCacheFilePath(resource_id,
                                     md5,
                                     GDataRootDirectory::CACHE_TYPE_PERSISTENT,
                                     CACHED_FILE_FROM_SERVER);
      }
    }
  }

  // Create symlink in pinned dir.
  if (create_symlink) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    GDataRootDirectory::CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(source_path,
                            dest_path,
                            file_operation_type,
                            symlink_path,
                            create_symlink);

  if (*error == base::PLATFORM_FILE_OK) {
    // Now that file operations have completed, update cache map.
    root_->UpdateCacheMap(resource_id, md5, sub_dir_type,
                          cache_state);
  }
}

void GDataFileSystem::UnpinOnIOThreadPool(const std::string& resource_id,
                                          const std::string& md5,
                                          FileOperationType file_operation_type,
                                          base::PlatformFileError* error) {
  DCHECK(error);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, md5);

  // Unpinning a file means its entry must exist in cache.
  if (!entry) {
    LOG(WARNING) << "Can't unpin a file that wasn't pinned or cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }

  // Entry exists in cache, determines source and destination paths.

  FilePath source_path;
  FilePath dest_path;
  GDataRootDirectory::CacheSubDirectoryType sub_dir_type =
      GDataRootDirectory::CACHE_TYPE_TMP;

  // If file is dirty or mounted, don't move it, so determine |dest_path| and
  // set |source_path| the same, because ModifyCacheState moves files if source
  // and destination are different.
  if (entry->IsDirty() || entry->IsMounted()) {
    sub_dir_type = GDataRootDirectory::CACHE_TYPE_PERSISTENT;
    DCHECK_EQ(sub_dir_type, entry->sub_dir_type);
    dest_path = GetCacheFilePath(resource_id,
                                 md5,
                                 entry->sub_dir_type,
                                 CACHED_FILE_LOCALLY_MODIFIED);
    source_path = dest_path;
  } else {
    // Gets the current path of the file in cache.
    source_path = GetCacheFilePath(resource_id,
                                   md5,
                                   entry->sub_dir_type,
                                   CACHED_FILE_FROM_SERVER);

    // If file was pinned but actual file blob still doesn't exist in cache,
    // don't need to move the file, so set |dest_path| to |source_path|, because
    // ModifyCacheState only moves files if source and destination are
    // different.
    if (entry->sub_dir_type == GDataRootDirectory::CACHE_TYPE_PINNED) {
      dest_path = source_path;
    } else {  // File exists, move it to tmp dir.
      dest_path = GetCacheFilePath(resource_id, md5,
                                   GDataRootDirectory::CACHE_TYPE_TMP,
                                   CACHED_FILE_FROM_SERVER);
    }
  }

  // If file was pinned, get absolute path of symlink in pinned dir so as to
  // remove it.
  FilePath symlink_path;
  if (entry->IsPinned()) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    GDataRootDirectory::CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(
      source_path,
      dest_path,
      file_operation_type,
      symlink_path,  // This will be deleted if it exists.
      false /* don't create symlink*/);

  if (*error == base::PLATFORM_FILE_OK) {
    // Now that file operations have completed, update cache map.
    int cache_state = GDataFile::ClearCachePinned(entry->cache_state);
    root_->UpdateCacheMap(resource_id, md5, sub_dir_type,
                          cache_state);
  }
}

void GDataFileSystem::MarkDirtyInCacheOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    FileOperationType file_operation_type,
    base::PlatformFileError* error,
    FilePath* cache_file_path) {
  DCHECK(error);
  DCHECK(cache_file_path);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.
  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, std::string());

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  if (!entry || entry->sub_dir_type == GDataRootDirectory::CACHE_TYPE_PINNED) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }

  // If a file is already dirty (i.e. MarkDirtyInCache was called before),
  // delete outgoing symlink if it exists.
  // TODO(benchan): We should only delete outgoing symlink if file is currently
  // not being uploaded.  However, for now, cache doesn't know if uploading of a
  // file is in progress.  Per zel, the upload process should be canceled before
  // MarkDirtyInCache is called again.
  if (entry->IsDirty()) {
    // The file must be in persistent dir.
    DCHECK_EQ(GDataRootDirectory::CACHE_TYPE_PERSISTENT, entry->sub_dir_type);

    // Determine symlink path in outgoing dir, so as to remove it.
    FilePath symlink_path = GetCacheFilePath(
        resource_id,
        std::string(),
        GDataRootDirectory::CACHE_TYPE_OUTGOING,
        CACHED_FILE_FROM_SERVER);

    // We're not moving files here, so simply use empty FilePath for both
    // |source_path| and |dest_path| because ModifyCacheState only move files
    // if source and destination are different.
    *error = ModifyCacheState(
        FilePath(),  // non-applicable source path
        FilePath(),  // non-applicable dest path
        file_operation_type,
        symlink_path,
        false /* don't create symlink */);

    // Determine current path of dirty file.
    if (*error == base::PLATFORM_FILE_OK) {
      *cache_file_path = GetCacheFilePath(
          resource_id,
          md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          CACHED_FILE_LOCALLY_MODIFIED);
    }
    return;
  }

  // Move file to persistent dir with new .local extension.

  // Get the current path of the file in cache.
  FilePath source_path = GetCacheFilePath(resource_id,
                                          md5,
                                          entry->sub_dir_type,
                                          CACHED_FILE_FROM_SERVER);

  // Determine destination path.
  GDataRootDirectory::CacheSubDirectoryType sub_dir_type =
      GDataRootDirectory::CACHE_TYPE_PERSISTENT;
  *cache_file_path = GetCacheFilePath(resource_id,
                                      md5,
                                      sub_dir_type,
                                      CACHED_FILE_LOCALLY_MODIFIED);

  // If file is pinned, update symlink in pinned dir.
  FilePath symlink_path;
  if (entry->IsPinned()) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    GDataRootDirectory::CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(
      source_path,
      *cache_file_path,
      file_operation_type,
      symlink_path,
      !symlink_path.empty() /* create symlink */);

  if (*error == base::PLATFORM_FILE_OK) {
    // Now that file operations have completed, update cache map.
    int cache_state = GDataFile::SetCacheDirty(entry->cache_state);
    root_->UpdateCacheMap(resource_id, md5, sub_dir_type,
                          cache_state);
  }
}

void GDataFileSystem::CommitDirtyInCacheOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    FileOperationType file_operation_type,
    base::PlatformFileError* error) {
  DCHECK(error);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.
  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, std::string());

  // Committing a file dirty means its entry and actual file blob must exist in
  // cache.
  if (!entry || entry->sub_dir_type == GDataRootDirectory::CACHE_TYPE_PINNED) {
    LOG(WARNING) << "Can't commit dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), commiting it dirty is an invalid operation.
  if (!entry->IsDirty()) {
    LOG(WARNING) << "Can't commit a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
    return;
  }

  // Dirty files must be in persistent dir.
  DCHECK_EQ(GDataRootDirectory::CACHE_TYPE_PERSISTENT, entry->sub_dir_type);

  // Create symlink in outgoing dir.
  FilePath symlink_path = GetCacheFilePath(
      resource_id,
      std::string(),
      GDataRootDirectory::CACHE_TYPE_OUTGOING,
      CACHED_FILE_FROM_SERVER);

  // Get target path of symlink i.e. current path of the file in cache.
  FilePath target_path = GetCacheFilePath(resource_id,
                                          md5,
                                          entry->sub_dir_type,
                                          CACHED_FILE_LOCALLY_MODIFIED);

  // Since there's no need to move files, use |target_path| for both
  // |source_path| and |dest_path|, because ModifyCacheState only moves files
  // if source and destination are different.
  *error = ModifyCacheState(target_path,  // source
                            target_path,  // destination
                            file_operation_type,
                            symlink_path,
                            true /* create symlink */);
}

void GDataFileSystem::ClearDirtyInCacheOnIOThreadPool(
    const std::string& resource_id,
    const std::string& md5,
    FileOperationType file_operation_type,
    base::PlatformFileError* error) {
  DCHECK(error);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  // |md5| is the new .<md5> extension to rename the file to.
  // So, search for entry in cache without comparing md5.
  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, std::string());

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  if (!entry || entry->sub_dir_type == GDataRootDirectory::CACHE_TYPE_PINNED) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << "res_id=" << resource_id
                 << ", md5=" << md5;
    *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), clearing its dirty state is an invalid operation.
  if (!entry->IsDirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
    return;
  }

  // File must be dirty and hence in persistent dir.
  DCHECK_EQ(GDataRootDirectory::CACHE_TYPE_PERSISTENT, entry->sub_dir_type);

  // Get the current path of the file in cache.
  FilePath source_path = GetCacheFilePath(resource_id,
                                          md5,
                                          entry->sub_dir_type,
                                          CACHED_FILE_LOCALLY_MODIFIED);

  // Determine destination path.
  // If file is pinned, move it to persistent dir with .md5 extension;
  // otherwise, move it to tmp dir with .md5 extension.
  GDataRootDirectory::CacheSubDirectoryType sub_dir_type =
      entry->IsPinned() ? GDataRootDirectory::CACHE_TYPE_PERSISTENT :
                          GDataRootDirectory::CACHE_TYPE_TMP;
  FilePath dest_path = GetCacheFilePath(resource_id,
                                        md5,
                                        sub_dir_type,
                                        CACHED_FILE_FROM_SERVER);

  // Delete symlink in outgoing dir.
  FilePath symlink_path = GetCacheFilePath(
      resource_id,
      std::string(),
      GDataRootDirectory::CACHE_TYPE_OUTGOING,
      CACHED_FILE_FROM_SERVER);

  *error = ModifyCacheState(
      source_path,
      dest_path,
      file_operation_type,
      symlink_path,
      false /* don't create symlink */);

  // If file is pinned, update symlink in pinned dir.
  if (*error == base::PLATFORM_FILE_OK && entry->IsPinned()) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    GDataRootDirectory::CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);

    // Since there's no moving of files here, use |dest_path| for both
    // |source_path| and |dest_path|, because ModifyCacheState only moves files
    // if source and destination are different.
    *error = ModifyCacheState(dest_path,  // source path
                             dest_path,  // destination path
                             file_operation_type,
                             symlink_path,
                             true /* create symlink */);
  }

  if (*error == base::PLATFORM_FILE_OK) {
    // Now that file operations have completed, update cache map.
    int cache_state = GDataFile::ClearCacheDirty(entry->cache_state);
    root_->UpdateCacheMap(resource_id, md5, sub_dir_type,
                          cache_state);
  }
}

void GDataFileSystem::RemoveFromCacheOnIOThreadPool(
    const std::string& resource_id,
    base::PlatformFileError* error) {
  DCHECK(error);

  // Lock to access cache map.
  base::AutoLock lock(lock_);

  // MD5 is not passed into RemoveFromCache and hence
  // RemoveFromCacheOnIOThreadPool, because we would delete all cache files
  // corresponding to <resource_id> regardless of the md5.
  // So, search for entry in cache without taking md5 into account.
  GDataRootDirectory::CacheEntry* entry = root_->GetCacheEntry(
      resource_id, std::string());

  // If entry doesn't exist or is dirty or mounted in cache, nothing to do.
  if (!entry || entry->IsDirty() || entry->IsMounted()) {
    DVLOG(1) << "Entry is "
             << (entry ? (entry->IsDirty() ? "dirty" : "mounted") :
                         "non-existent")
             << " in cache, not removing";
    *error = base::PLATFORM_FILE_OK;
    return;
  }

  // Determine paths to delete all cache versions of |resource_id| in
  // persistent, tmp and pinned directories.
  std::vector<FilePath> paths_to_delete;

  // For files in persistent and tmp dirs, delete files that match
  // "<resource_id>.*".
  paths_to_delete.push_back(GetCacheFilePath(
      resource_id,
      kWildCard,
      GDataRootDirectory::CACHE_TYPE_PERSISTENT,
      CACHED_FILE_FROM_SERVER));
  paths_to_delete.push_back(GetCacheFilePath(
      resource_id,
      kWildCard,
      GDataRootDirectory::CACHE_TYPE_TMP,
      CACHED_FILE_FROM_SERVER));

  // For pinned files, filename is "<resource_id>" with no extension, so delete
  // "<resource_id>".
  paths_to_delete.push_back(GetCacheFilePath(
      resource_id,
      std::string(),
      GDataRootDirectory::CACHE_TYPE_PINNED,
      CACHED_FILE_FROM_SERVER));

  // Don't delete locally modified (i.e. dirty and possibly outgoing) files.
  // Since we're not deleting outgoing symlinks, we don't need to append
  // outgoing path to |paths_to_delete|.
  FilePath path_to_keep = GetCacheFilePath(
      resource_id,
      std::string(),
      GDataRootDirectory::CACHE_TYPE_PERSISTENT,
      CACHED_FILE_LOCALLY_MODIFIED);

  for (size_t i = 0; i < paths_to_delete.size(); ++i) {
    DeleteFilesSelectively(paths_to_delete[i], path_to_keep);
  }

  // Now that all file operations have completed, remove from cache map.
  root_->RemoveFromCacheMap(resource_id);

  *error = base::PLATFORM_FILE_OK;
}

//=== GDataFileSystem: Cache callbacks for tasks that ran on io thread pool ====

void GDataFileSystem::OnFilePinned(base::PlatformFileError* error,
                                   const std::string& resource_id,
                                   const std::string& md5,
                                   const CacheOperationCallback& callback) {
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);

  if (*error == base::PLATFORM_FILE_OK)
    NotifyFilePinned(resource_id, md5);
}

void GDataFileSystem::OnFileUnpinned(base::PlatformFileError* error,
                                     const std::string& resource_id,
                                     const std::string& md5,
                                     const CacheOperationCallback& callback) {
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);

  if (*error == base::PLATFORM_FILE_OK)
    NotifyFileUnpinned(resource_id, md5);

  // Now the file is moved from "persistent" to "tmp" directory.
  // It's a chance to free up space if needed.
  bool* has_enough_space = new bool(false);
  PostBlockingPoolSequencedTask(
      FROM_HERE,
      base::Bind(&GDataFileSystem::FreeDiskSpaceIfNeeded,
                 base::Unretained(this),
                 base::Owned(has_enough_space)));
}

//============= GDataFileSystem: internal helper functions =====================

void GDataFileSystem::ScanCacheDirectory(
    GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
    GDataRootDirectory::CacheMap* cache_map) {
  file_util::FileEnumerator enumerator(
      GetCacheDirectoryPath(sub_dir_type),
      false,  // not recursive
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::SHOW_SYM_LINKS),
      kWildCard);
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // Extract resource_id and md5 from filename.
    std::string resource_id;
    std::string md5;
    std::string extra_extension;
    util::ParseCacheFilePath(current, &resource_id, &md5, &extra_extension);

    // Determine cache state.
    int cache_state = GDataFile::CACHE_STATE_NONE;
    // If we're scanning pinned directory and if entry already exists, just
    // update its pinned state.
    if (sub_dir_type == GDataRootDirectory::CACHE_TYPE_PINNED) {
      GDataRootDirectory::CacheMap::iterator iter =
          cache_map->find(resource_id);
      if (iter != cache_map->end()) {  // Entry exists, update pinned state.
        GDataRootDirectory::CacheEntry* entry = iter->second;
        entry->cache_state = GDataFile::SetCachePinned(entry->cache_state);
        continue;
      }
      // Entry doesn't exist, this is a special symlink that refers to
      // /dev/null; follow through to create an entry with the PINNED but not
      // PRESENT state.
      cache_state = GDataFile::SetCachePinned(cache_state);
    } else if (sub_dir_type == GDataRootDirectory::CACHE_TYPE_OUTGOING) {
      // If we're scanning outgoing directory, entry must exist, update its
      // dirty state.
      // If entry doesn't exist, it's a logic error from previous execution,
      // ignore this outgoing symlink and move on.
      GDataRootDirectory::CacheMap::iterator iter =
          cache_map->find(resource_id);
      if (iter != cache_map->end()) {  // Entry exists, update dirty state.
        GDataRootDirectory::CacheEntry* entry = iter->second;
        entry->cache_state = GDataFile::SetCacheDirty(entry->cache_state);
      } else {
        NOTREACHED() << "Dirty cache file MUST have actual file blob";
      }
      continue;
    } else if (extra_extension == kMountedArchiveFileExtension) {
      // Mounted archives in cache should be unmounted upon logout/shutdown.
      // But if we encounter a mounted file at start, delete it and create an
      // entry with not PRESENT state.
      DCHECK(sub_dir_type == GDataRootDirectory::CACHE_TYPE_PERSISTENT);
      file_util::Delete(current, false);
    } else {
      // Scanning other directories means that cache file is actually present.
      cache_state = GDataFile::SetCachePresent(cache_state);
    }

    // Create and insert new entry into cache map.
    GDataRootDirectory::CacheEntry* entry = new GDataRootDirectory::CacheEntry(
        md5, sub_dir_type, cache_state);
    cache_map->insert(std::make_pair(resource_id, entry));
  }
}

void GDataFileSystem::GetFileFromCacheByResourceIdAndMd5(
    const std::string& resource_id,
    const std::string& md5,
    const GetFileFromCacheCallback& callback) {
  InitializeCacheIfNecessary();

  base::PlatformFileError* error =
      new base::PlatformFileError(base::PLATFORM_FILE_OK);
  FilePath* cache_file_path = new FilePath;
  PostBlockingPoolSequencedTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataFileSystem::GetFileFromCacheOnIOThreadPool,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 error,
                 cache_file_path),
      base::Bind(&RunGetFileFromCacheCallbackHelper,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5,
                 base::Owned(cache_file_path)));
}

void GDataFileSystem::RunTaskOnIOThreadPool(const base::Closure& task) {
  task.Run();

  {
    base::AutoLock lock(num_pending_tasks_lock_);
    --num_pending_tasks_;
    // Signal when the last task is completed.
    if (num_pending_tasks_ == 0)
      on_io_completed_->Signal();
  }
}

void GDataFileSystem::PostBlockingPoolSequencedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  PostBlockingPoolSequencedTaskAndReply(
      from_here,
      task,
      base::Bind(&base::DoNothing));
}

void GDataFileSystem::PostBlockingPoolSequencedTaskAndReply(
    const tracked_objects::Location& from_here,
    const base::Closure& request_task,
    const base::Closure& reply_task) {
  {
    // Note that we cannot use |lock_| as lock_ can be held before this
    // function is called (i.e. InitializeCacheIfNecessary does).
    base::AutoLock lock(num_pending_tasks_lock_);

    // Initiate the sequenced task. We should Reset() here rather than on the
    // blocking thread pool, as Reset() will cause a deadlock if it's called
    // while Wait() is being called in the destructor.
    //
    // Signaling on_io_completed_ is closely coupled with number of pending
    // tasks. We signal it when the number decreases to 0. Because of that, we
    // should do the reset under |num_pending_tasks_lock_|. Otherwise, we could
    // get in trouble if |num_pending_tasks_| gets decreased after the event is
    // reset, but before we increase the number here.
    on_io_completed_->Reset();

    ++num_pending_tasks_;
  }
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  const bool posted = pool->GetSequencedTaskRunner(sequence_token_)->
      PostTaskAndReply(
          from_here,
          base::Bind(&GDataFileSystem::RunTaskOnIOThreadPool,
                     base::Unretained(this),
                     request_task),
          reply_task);
  DCHECK(posted);
}

void GDataFileSystem::InitializePreferenceObserver() {
  pref_registrar_.reset(new PrefChangeRegistrar());
  pref_registrar_->Init(profile_->GetPrefs());
  pref_registrar_->Add(prefs::kDisableGDataHostedFiles, this);
}

void SetFreeDiskSpaceGetterForTesting(FreeDiskSpaceGetterInterface* getter) {
  delete global_free_disk_getter_for_testing;  // Safe to delete NULL;
  global_free_disk_getter_for_testing = getter;
}

}  // namespace gdata
