// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include <string>
#include <vector>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/file_write_helper.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/escape.h"
#include "third_party/libxml/chromium/libxml_utils.h"

using content::BrowserThread;

namespace gdata {
namespace util {

namespace {

const char kGDataSpecialRootPath[] = "/special";

const char kGDataMountPointPath[] = "/special/drive";

const FilePath::CharType* kGDataMountPointPathComponents[] = {
  "/", "special", "drive"
};

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

GDataFileSystemInterface* GetGDataFileSystem(Profile* profile) {
  GDataSystemService* system_service =
      GDataSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_system() : NULL;
}

DriveCache* GetDriveCache(Profile* profile) {
  GDataSystemService* system_service =
      GDataSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->cache() : NULL;
}

FileWriteHelper* GetFileWriteHelper(Profile* profile) {
  GDataSystemService* system_service =
      GDataSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_write_helper() : NULL;
}

void GetHostedDocumentURLBlockingThread(const FilePath& drive_cache_path,
                                        GURL* url) {
  std::string json;
  if (!file_util::ReadFileToString(drive_cache_path, &json)) {
    NOTREACHED() << "Unable to read file " << drive_cache_path.value();
    return;
  }
  DVLOG(1) << "Hosted doc content " << json;
  scoped_ptr<base::Value> val(base::JSONReader::Read(json));
  base::DictionaryValue* dict_val;
  if (!val.get() || !val->GetAsDictionary(&dict_val)) {
    NOTREACHED() << "Parse failure for " << json;
    return;
  }
  std::string edit_url;
  if (!dict_val->GetString("url", &edit_url)) {
    NOTREACHED() << "url field doesn't exist in " << json;
    return;
  }
  *url = GURL(edit_url);
  DVLOG(1) << "edit url " << *url;
}

void OpenEditURLUIThread(Profile* profile, const GURL* edit_url) {
  Browser* browser = browser::FindLastActiveWithProfile(profile);
  if (browser) {
    browser->OpenURL(content::OpenURLParams(*edit_url, content::Referrer(),
        CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  }
}

// Invoked upon completion of GetEntryInfoByResourceId initiated by
// ModifyGDataFileResourceUrl.
void OnGetEntryInfoByResourceId(Profile* profile,
                                const std::string& resource_id,
                                DriveFileError error,
                                const FilePath& /* gdata_file_path */,
                                scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK)
    return;

  DCHECK(entry_proto.get());
  const std::string& base_name = entry_proto->base_name();
  const GURL edit_url = GetFileResourceUrl(resource_id, base_name);
  OpenEditURLUIThread(profile, &edit_url);
  DVLOG(1) << "OnFindEntryByResourceId " << edit_url;
}

// Invoked upon completion of GetEntryInfoByPath initiated by
// InsertDriveCachePathPermissions.
void OnGetEntryInfoForInsertDriveCachePathsPermissions(
    Profile* profile,
    std::vector<std::pair<FilePath, int> >* cache_paths,
    const base::Closure& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(profile);
  DCHECK(cache_paths);
  DCHECK(!callback.is_null());

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  DriveCache* cache = GetDriveCache(profile);
  if (!cache || error != DRIVE_FILE_OK) {
    callback.Run();
    return;
  }

  DCHECK(entry_proto.get());
  const std::string& resource_id = entry_proto->resource_id();
  const std::string& file_md5 = entry_proto->file_specific_info().file_md5();

  // We check permissions for raw cache file paths only for read-only
  // operations (when fileEntry.file() is called), so read only permissions
  // should be sufficient for all cache paths. For the rest of supported
  // operations the file access check is done for drive/ paths.
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_PERSISTENT,
          DriveCache::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));
  // TODO(tbarzic): When we start supporting openFile operation, we may have to
  // change permission for localy modified files to match handler's permissions.
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_PERSISTENT,
          DriveCache::CACHED_FILE_LOCALLY_MODIFIED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_PERSISTENT,
          DriveCache::CACHED_FILE_MOUNTED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_TMP,
          DriveCache::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));

  callback.Run();
}

bool ParseTimezone(const base::StringPiece& timezone,
                   bool ahead,
                   int* out_offset_to_utc_in_minutes) {
  DCHECK(out_offset_to_utc_in_minutes);

  std::vector<base::StringPiece> parts;
  int num_of_token = Tokenize(timezone, ":", &parts);

  int hour = 0;
  if (!base::StringToInt(parts[0], &hour))
    return false;

  int minute = 0;
  if (num_of_token > 1 && !base::StringToInt(parts[1], &minute))
    return false;

  *out_offset_to_utc_in_minutes = (hour * 60 + minute) * (ahead ? +1 : -1);
  return true;
}

}  // namespace

const FilePath& GetGDataMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataMountPointPath)));
  return gdata_mount_path;
}

const std::string& GetGDataMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, gdata_mount_path_string,
      (kGDataMountPointPath));
  return gdata_mount_path_string;
}

const FilePath& GetSpecialRemoteRootPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataSpecialRootPath)));
  return gdata_mount_path;
}

GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name) {
  std::string url(base::StringPrintf(
      "%s:%s",
      chrome::kDriveScheme,
      net::EscapePath(resource_id).c_str()));
  return GURL(url);
}

void ModifyGDataFileResourceUrl(Profile* profile,
                                const FilePath& drive_cache_path,
                                GURL* url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileSystemInterface* file_system = GetGDataFileSystem(profile);
  if (!file_system)
    return;
  DriveCache* cache = GetDriveCache(profile);
  if (!cache)
    return;

  if (cache->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP_DOCUMENTS).
      IsParent(drive_cache_path)) {
    // Handle hosted documents. The edit url is in the temporary file, so we
    // read it on a blocking thread.
    GURL* edit_url = new GURL();
    content::BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&GetHostedDocumentURLBlockingThread,
                   drive_cache_path, edit_url),
        base::Bind(&OpenEditURLUIThread, profile, base::Owned(edit_url)));
    *url = GURL();
  } else if (cache->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP).
                 IsParent(drive_cache_path) ||
             cache->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_PERSISTENT).
                 IsParent(drive_cache_path)) {
    // Handle all other gdata files.
    const std::string resource_id =
        drive_cache_path.BaseName().RemoveExtension().AsUTF8Unsafe();
    file_system->GetEntryInfoByResourceId(
        resource_id,
        base::Bind(&OnGetEntryInfoByResourceId,
                   profile,
                   resource_id));
    *url = GURL();
  }
}

bool IsUnderGDataMountPoint(const FilePath& path) {
  return GetGDataMountPointPath() == path ||
         GetGDataMountPointPath().IsParent(path);
}

FilePath ExtractGDataPath(const FilePath& path) {
  if (!IsUnderGDataMountPoint(path))
    return FilePath();

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'drive'.
  FilePath extracted;
  for (size_t i = arraysize(kGDataMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
}

void InsertDriveCachePathsPermissions(
    Profile* profile,
    scoped_ptr<std::vector<FilePath> > gdata_paths,
    std::vector<std::pair<FilePath, int> >* cache_paths,
    const base::Closure& callback) {
  DCHECK(profile);
  DCHECK(gdata_paths.get());
  DCHECK(cache_paths);
  DCHECK(!callback.is_null());

  GDataFileSystemInterface* file_system = GetGDataFileSystem(profile);
  if (!file_system || gdata_paths->empty()) {
    callback.Run();
    return;
  }

  // Remove one file path entry from the back of the input vector |gdata_paths|.
  FilePath gdata_path = gdata_paths->back();
  gdata_paths->pop_back();

  // Call GetEntryInfoByPath() to get file info for |gdata_path| then insert
  // all possible cache paths to the output vector |cache_paths|.
  // Note that we can only process one file path at a time. Upon completion
  // of OnGetEntryInfoForInsertDriveCachePathsPermissions(), we recursively call
  // InsertDriveCachePathsPermissions() to process the next file path from the
  // back of the input vector |gdata_paths| until it is empty.
  file_system->GetEntryInfoByPath(
      gdata_path,
      base::Bind(&OnGetEntryInfoForInsertDriveCachePathsPermissions,
                 profile,
                 cache_paths,
                 base::Bind(&InsertDriveCachePathsPermissions,
                             profile,
                             base::Passed(&gdata_paths),
                             cache_paths,
                             callback)));
}

bool IsGDataAvailable(Profile* profile) {
  if (!chromeos::UserManager::Get()->IsUserLoggedIn() ||
      chromeos::UserManager::Get()->IsLoggedInAsGuest() ||
      chromeos::UserManager::Get()->IsLoggedInAsDemoUser())
    return false;

  // Do not allow GData for incognito windows / guest mode.
  if (profile->IsOffTheRecord())
    return false;

  // Disable gdata if preference is set.  This can happen with commandline flag
  // --disable-gdata or enterprise policy, or probably with user settings too
  // in the future.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return false;

  return true;
}

std::string EscapeCacheFileName(const std::string& filename) {
  // This is based on net/base/escape.cc: net::(anonymous namespace)::Escape
  std::string escaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' || c == '.' || c == '/') {
      base::StringAppendF(&escaped, "%%%02X", c);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string UnescapeCacheFileName(const std::string& filename) {
  std::string unescaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' && i + 2 < filename.length()) {
      c = (HexDigitToInt(filename[i + 1]) << 4) +
           HexDigitToInt(filename[i + 2]);
      i += 2;
    }
    unescaped.push_back(c);
  }
  return unescaped;
}

void ParseCacheFilePath(const FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension) {
  DCHECK(resource_id);
  DCHECK(md5);
  DCHECK(extra_extension);

  // Extract up to two extensions from the right.
  FilePath base_name = path.BaseName();
  const int kNumExtensionsToExtract = 2;
  std::vector<FilePath::StringType> extensions;
  for (int i = 0; i < kNumExtensionsToExtract; ++i) {
    FilePath::StringType extension = base_name.Extension();
    if (!extension.empty()) {
      // FilePath::Extension returns ".", so strip it.
      extension = UnescapeCacheFileName(extension.substr(1));
      base_name = base_name.RemoveExtension();
      extensions.push_back(extension);
    } else {
      break;
    }
  }

  // The base_name here is already stripped of extensions in the loop above.
  *resource_id = UnescapeCacheFileName(base_name.value());

  // Assign the extracted extensions to md5 and extra_extension.
  int extension_count = extensions.size();
  *md5 = (extension_count > 0) ? extensions[extension_count - 1] :
                                 std::string();
  *extra_extension = (extension_count > 1) ? extensions[extension_count - 2] :
                                             std::string();
}

bool IsDriveV2ApiEnabled() {
  static bool enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDriveV2Api);
  return enabled;
}

base::PlatformFileError DriveFileErrorToPlatformError(
    gdata::DriveFileError error) {
  switch (error) {
    case gdata::DRIVE_FILE_OK:
      return base::PLATFORM_FILE_OK;

    case gdata::DRIVE_FILE_ERROR_FAILED:
      return base::PLATFORM_FILE_ERROR_FAILED;

    case gdata::DRIVE_FILE_ERROR_IN_USE:
      return base::PLATFORM_FILE_ERROR_IN_USE;

    case gdata::DRIVE_FILE_ERROR_EXISTS:
      return base::PLATFORM_FILE_ERROR_EXISTS;

    case gdata::DRIVE_FILE_ERROR_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;

    case gdata::DRIVE_FILE_ERROR_ACCESS_DENIED:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

    case gdata::DRIVE_FILE_ERROR_TOO_MANY_OPENED:
      return base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED;

    case gdata::DRIVE_FILE_ERROR_NO_MEMORY:
      return base::PLATFORM_FILE_ERROR_NO_MEMORY;

    case gdata::DRIVE_FILE_ERROR_NO_SPACE:
      return base::PLATFORM_FILE_ERROR_NO_SPACE;

    case gdata::DRIVE_FILE_ERROR_NOT_A_DIRECTORY:
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

    case gdata::DRIVE_FILE_ERROR_INVALID_OPERATION:
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

    case gdata::DRIVE_FILE_ERROR_SECURITY:
      return base::PLATFORM_FILE_ERROR_SECURITY;

    case gdata::DRIVE_FILE_ERROR_ABORT:
      return base::PLATFORM_FILE_ERROR_ABORT;

    case gdata::DRIVE_FILE_ERROR_NOT_A_FILE:
      return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

    case gdata::DRIVE_FILE_ERROR_NOT_EMPTY:
      return base::PLATFORM_FILE_ERROR_NOT_EMPTY;

    case gdata::DRIVE_FILE_ERROR_INVALID_URL:
      return base::PLATFORM_FILE_ERROR_INVALID_URL;

    case gdata::DRIVE_FILE_ERROR_NO_CONNECTION:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }

  NOTREACHED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

bool GetTimeFromString(const base::StringPiece& raw_value,
                       base::Time* parsed_time) {
  base::StringPiece date;
  base::StringPiece time_and_tz;
  base::StringPiece time;
  base::Time::Exploded exploded = {0};
  bool has_timezone = false;
  int offset_to_utc_in_minutes = 0;

  // Splits the string into "date" part and "time" part.
  {
    std::vector<base::StringPiece> parts;
    if (Tokenize(raw_value, "T", &parts) != 2)
      return false;
    date = parts[0];
    time_and_tz = parts[1];
  }

  // Parses timezone suffix on the time part if available.
  {
    std::vector<base::StringPiece> parts;
    if (time_and_tz[time_and_tz.size() - 1] == 'Z') {
      // Timezone is 'Z' (UTC)
      has_timezone = true;
      offset_to_utc_in_minutes = 0;
      time = time_and_tz;
      time.remove_suffix(1);
    } else if (Tokenize(time_and_tz, "+", &parts) == 2) {
      // Timezone is "+hh:mm" format
      if (!ParseTimezone(parts[1], true, &offset_to_utc_in_minutes))
        return false;
      has_timezone = true;
      time = parts[0];
    } else if (Tokenize(time_and_tz, "-", &parts) == 2) {
      // Timezone is "-hh:mm" format
      if (!ParseTimezone(parts[1], false, &offset_to_utc_in_minutes))
        return false;
      has_timezone = true;
      time = parts[0];
    } else {
      // No timezone (uses local timezone)
      time = time_and_tz;
    }
  }

  // Parses the date part.
  {
    std::vector<base::StringPiece> parts;
    if (Tokenize(date, "-", &parts) != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  // Parses the time part.
  {
    std::vector<base::StringPiece> parts;
    int num_of_token = Tokenize(time, ":", &parts);
    if (num_of_token != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.hour) ||
        !base::StringToInt(parts[1], &exploded.minute)) {
      return false;
    }

    std::vector<base::StringPiece> seconds_parts;
    int num_of_seconds_token = Tokenize(parts[2], ".", &seconds_parts);
    if (num_of_seconds_token >= 3)
      return false;

    if (!base::StringToInt(seconds_parts[0], &exploded.second))
        return false;

    // Only accept milli-seconds (3-digits).
    if (num_of_seconds_token > 1 &&
        seconds_parts[1].length() == 3 &&
        !base::StringToInt(seconds_parts[1], &exploded.millisecond)) {
      return false;
    }
  }

  exploded.day_of_week = 0;
  if (!exploded.HasValidValues())
    return false;

  if (has_timezone) {
    *parsed_time = base::Time::FromUTCExploded(exploded);
    if (offset_to_utc_in_minutes != 0)
      *parsed_time -= base::TimeDelta::FromMinutes(offset_to_utc_in_minutes);
  } else {
    *parsed_time = base::Time::FromLocalExploded(exploded);
  }

  return true;
}

std::string FormatTimeAsString(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}

std::string FormatTimeAsStringLocaltime(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);

  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}

void PrepareWritableFileAndRun(Profile* profile,
                               const FilePath& path,
                               const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsUnderGDataMountPoint(path)) {
    FileWriteHelper* file_write_helper = GetFileWriteHelper(profile);
    if (!file_write_helper)
      return;
    FilePath remote_path(ExtractGDataPath(path));
    file_write_helper->PrepareWritableFileAndRun(remote_path, callback);
  } else {
    if (!callback.is_null()) {
      content::BrowserThread::GetBlockingPool()->PostTask(
          FROM_HERE, base::Bind(callback, DRIVE_FILE_OK, path));
    }
  }
}

DriveFileError GDataToDriveFileError(GDataErrorCode status) {
  switch (status) {
    case HTTP_SUCCESS:
    case HTTP_CREATED:
      return DRIVE_FILE_OK;
    case HTTP_UNAUTHORIZED:
    case HTTP_FORBIDDEN:
      return DRIVE_FILE_ERROR_ACCESS_DENIED;
    case HTTP_NOT_FOUND:
      return DRIVE_FILE_ERROR_NOT_FOUND;
    case GDATA_PARSE_ERROR:
    case GDATA_FILE_ERROR:
      return DRIVE_FILE_ERROR_ABORT;
    case GDATA_NO_CONNECTION:
      return DRIVE_FILE_ERROR_NO_CONNECTION;
    default:
      return DRIVE_FILE_ERROR_FAILED;
  }
}

void PostBlockingPoolSequencedTask(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool posted = blocking_task_runner->PostTask(from_here, task);
  DCHECK(posted);
}

void PostBlockingPoolSequencedTaskAndReply(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Closure& request_task,
    const base::Closure& reply_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool posted = blocking_task_runner->PostTaskAndReply(
      from_here, request_task, reply_task);
  DCHECK(posted);
}

}  // namespace util
}  // namespace gdata
