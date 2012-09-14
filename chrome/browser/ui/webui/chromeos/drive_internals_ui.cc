// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/gdata/auth_service.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Gets metadata of all files and directories in |root_path|
// recursively. Stores the result as a list of dictionaries like:
//
// [{ path: 'GCache/v1/tmp/<resource_id>',
//    size: 12345,
//    is_directory: false,
//    last_modified: '2005-08-09T09:57:00-08:00',
//  },...]
//
// The list is sorted by the path.
void GetGCacheContents(const FilePath& root_path,
                       base::ListValue* gcache_contents,
                       base::DictionaryValue* gcache_summary) {
  using file_util::FileEnumerator;
  // Use this map to sort the result list by the path.
  std::map<FilePath, DictionaryValue*> files;

  const int options = (file_util::FileEnumerator::FILES |
                       file_util::FileEnumerator::DIRECTORIES |
                       file_util::FileEnumerator::SHOW_SYM_LINKS);
  FileEnumerator enumerator(root_path, true /* recursive */, options);

  int64 total_size = 0;
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    FileEnumerator::FindInfo find_info;
    enumerator.GetFindInfo(&find_info);
    int64 size = FileEnumerator::GetFilesize(find_info);
    const bool is_directory = FileEnumerator::IsDirectory(find_info);
    const bool is_symbolic_link =
        file_util::IsLink(FileEnumerator::GetFilename(find_info));
    const base::Time last_modified =
        FileEnumerator::GetLastModifiedTime(find_info);

    base::DictionaryValue* entry = new base::DictionaryValue;
    entry->SetString("path", current.value());
    // Use double instead of integer for large files.
    entry->SetDouble("size", size);
    entry->SetBoolean("is_directory", is_directory);
    entry->SetBoolean("is_symbolic_link", is_symbolic_link);
    entry->SetString("last_modified",
                     gdata::util::FormatTimeAsStringLocaltime(last_modified));
    files[current] = entry;

    total_size += size;
  }

  // Convert |files| into |gcache_contents|.
  for (std::map<FilePath, DictionaryValue*>::const_iterator
           iter = files.begin(); iter != files.end(); ++iter) {
    gcache_contents->Append(iter->second);
  }

  gcache_summary->SetDouble("total_size", total_size);
}

// Gets the available disk space for the path |home_path|.
void GetFreeDiskSpace(const FilePath& home_path,
                      base::DictionaryValue* local_storage_summary) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_storage_summary);

  const int64 free_space = base::SysInfo::AmountOfFreeDiskSpace(home_path);
  local_storage_summary->SetDouble("free_space", free_space);
}

// Formats |entry| into text.
std::string FormatEntry(const FilePath& path,
                        const gdata::DriveEntryProto& entry) {
  using base::StringAppendF;
  using gdata::util::FormatTimeAsString;

  std::string out;
  StringAppendF(&out, "%s\n", path.AsUTF8Unsafe().c_str());
  StringAppendF(&out, "  title: %s\n", entry.title().c_str());
  StringAppendF(&out, "  resource_id: %s\n", entry.resource_id().c_str());
  StringAppendF(&out, "  edit_url: %s\n", entry.edit_url().c_str());
  StringAppendF(&out, "  content_url: %s\n", entry.content_url().c_str());
  StringAppendF(&out, "  parent_resource_id: %s\n",
                entry.parent_resource_id().c_str());
  StringAppendF(&out, "  upload_url: %s\n", entry.upload_url().c_str());

  const gdata::PlatformFileInfoProto& file_info = entry.file_info();
  StringAppendF(&out, "  file_info\n");
  StringAppendF(&out, "    size: %"PRId64"\n", file_info.size());
  StringAppendF(&out, "    is_directory: %d\n", file_info.is_directory());
  StringAppendF(&out, "    is_symbolic_link: %d\n",
                file_info.is_symbolic_link());

  const base::Time last_modified = base::Time::FromInternalValue(
      file_info.last_modified());
  const base::Time last_accessed = base::Time::FromInternalValue(
      file_info.last_accessed());
  const base::Time creation_time = base::Time::FromInternalValue(
      file_info.creation_time());
  StringAppendF(&out, "    last_modified: %s\n",
                FormatTimeAsString(last_modified).c_str());
  StringAppendF(&out, "    last_accessed: %s\n",
                FormatTimeAsString(last_accessed).c_str());
  StringAppendF(&out, "    creation_time: %s\n",
                FormatTimeAsString(creation_time).c_str());

  if (entry.has_file_specific_info()) {
    const gdata::DriveFileSpecificInfo& file_specific_info =
        entry.file_specific_info();
    StringAppendF(&out, "    thumbnail_url: %s\n",
                  file_specific_info.thumbnail_url().c_str());
    StringAppendF(&out, "    alternate_url: %s\n",
                  file_specific_info.alternate_url().c_str());
    StringAppendF(&out, "    content_mime_type: %s\n",
                  file_specific_info.content_mime_type().c_str());
    StringAppendF(&out, "    file_md5: %s\n",
                  file_specific_info.file_md5().c_str());
    StringAppendF(&out, "    document_extension: %s\n",
                  file_specific_info.document_extension().c_str());
    StringAppendF(&out, "    is_hosted_document: %d\n",
                  file_specific_info.is_hosted_document());
  }

  return out;
}

// Class to handle messages from chrome://drive-internals.
class DriveInternalsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DriveInternalsWebUIHandler()
      : num_pending_reads_(0),
        weak_ptr_factory_(this) {
  }

  virtual ~DriveInternalsWebUIHandler() {
  }

 private:
  // WebUIMessageHandler override.
  virtual void RegisterMessages() OVERRIDE;

  // Returns a DriveSystemService.
  gdata::DriveSystemService* GetSystemService();

  // Called when the page is first loaded.
  void OnPageLoaded(const base::ListValue* args);

  // Called when GetGCacheContents() is complete.
  void OnGetGCacheContents(base::ListValue* gcache_contents,
                           base::DictionaryValue* cache_summary);

  // Called when ReadDirectoryByPath() is complete.
  void OnReadDirectoryByPath(const FilePath& parent_path,
                             gdata::DriveFileError error,
                             bool hide_hosted_documents,
                             scoped_ptr<gdata::DriveEntryProtoVector> entries);

  // Called when GetResourceIdsOfAllFilesOnUIThread() is complete.
  void OnGetResourceIdsOfAllFiles(
      const std::vector<std::string>& resource_ids);

  // Called when GetCacheEntryOnUIThread() is complete.
  void OnGetCacheEntry(const std::string& resource_id,
                       bool success,
                       const gdata::DriveCacheEntry& cache_entry);

  // Called when GetFreeDiskSpace() is complete.
  void OnGetFreeDiskSpace(base::DictionaryValue* local_storage_summary);

  // Called when the page requests periodic update.
  void OnPeriodicUpdate(const base::ListValue* args);

  // Updates the summary about in-flight operations.
  void UpdateInFlightOperations(
      const gdata::DriveServiceInterface* drive_service);

  // The number of pending ReadDirectoryByPath() calls.
  int num_pending_reads_;
  base::WeakPtrFactory<DriveInternalsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveInternalsWebUIHandler);
};

void DriveInternalsWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::Bind(&DriveInternalsWebUIHandler::OnPageLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "periodicUpdate",
      base::Bind(&DriveInternalsWebUIHandler::OnPeriodicUpdate,
                 weak_ptr_factory_.GetWeakPtr()));
}

gdata::DriveSystemService* DriveInternalsWebUIHandler::GetSystemService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return gdata::DriveSystemServiceFactory::GetForProfile(profile);
}

void DriveInternalsWebUIHandler::OnPageLoaded(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata::DriveSystemService* system_service = GetSystemService();
  // |system_service| may be NULL in the guest/incognito mode.
  if (!system_service)
    return;

  gdata::DriveServiceInterface* drive_service = system_service->drive_service();
  DCHECK(drive_service);

  // Update the auth status section.
  base::DictionaryValue auth_status;
  auth_status.SetBoolean("has-refresh-token",
                         drive_service->HasRefreshToken());
  auth_status.SetBoolean("has-access-token",
                         drive_service->HasAccessToken());
  web_ui()->CallJavascriptFunction("updateAuthStatus", auth_status);

  // Start updating the GCache contents section.
  Profile* profile = Profile::FromWebUI(web_ui());
  const FilePath root_path =
      gdata::DriveCache::GetCacheRootPath(profile);
  base::ListValue* gcache_contents = new ListValue;
  base::DictionaryValue* gcache_summary = new DictionaryValue;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&GetGCacheContents,
                 root_path,
                 gcache_contents,
                 gcache_summary),
      base::Bind(&DriveInternalsWebUIHandler::OnGetGCacheContents,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(gcache_contents),
                 base::Owned(gcache_summary)));

  // Propagate the amount of local free space in bytes.
  FilePath home_path;
  if (PathService::Get(base::DIR_HOME, &home_path)) {
    base::DictionaryValue* local_storage_summary = new DictionaryValue;
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&GetFreeDiskSpace, home_path, local_storage_summary),
        base::Bind(&DriveInternalsWebUIHandler::OnGetFreeDiskSpace,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(local_storage_summary)));
  } else {
    LOG(ERROR) << "Home directory not found";
  }
}

void DriveInternalsWebUIHandler::OnGetGCacheContents(
    base::ListValue* gcache_contents,
    base::DictionaryValue* gcache_summary) {
  DCHECK(gcache_contents);
  DCHECK(gcache_summary);
  web_ui()->CallJavascriptFunction("updateGCacheContents",
                                   *gcache_contents,
                                   *gcache_summary);

  // Start updating the file system tree section, if we have access token.
  gdata::DriveSystemService* system_service = GetSystemService();
  if (!system_service->drive_service()->HasAccessToken())
    return;

  // Start rendering the file system tree as text.
  const FilePath root_path = FilePath(gdata::kDriveRootDirectory);
  ++num_pending_reads_;
  system_service->file_system()->ReadDirectoryByPath(
      root_path,
      base::Bind(&DriveInternalsWebUIHandler::OnReadDirectoryByPath,
                 weak_ptr_factory_.GetWeakPtr(),
                 root_path));
}

void DriveInternalsWebUIHandler::OnReadDirectoryByPath(
    const FilePath& parent_path,
    gdata::DriveFileError error,
    bool hide_hosted_documents,
    scoped_ptr<gdata::DriveEntryProtoVector> entries) {
  --num_pending_reads_;
  if (error == gdata::DRIVE_FILE_OK) {
    DCHECK(entries.get());

    std::string file_system_as_text;
    for (size_t i = 0; i < entries->size(); ++i) {
      const gdata::DriveEntryProto& entry = (*entries)[i];
      const FilePath current_path = parent_path.Append(
          FilePath::FromUTF8Unsafe(entry.base_name()));

      file_system_as_text.append(FormatEntry(current_path, entry) + "\n");

      if (entry.file_info().is_directory()) {
        ++num_pending_reads_;
        GetSystemService()->file_system()->ReadDirectoryByPath(
            current_path,
            base::Bind(&DriveInternalsWebUIHandler::OnReadDirectoryByPath,
                       weak_ptr_factory_.GetWeakPtr(),
                       current_path));
      }
    }

    // There may be pending ReadDirectoryByPath() calls, but we can update
    // the page with what we have now. This results in progressive
    // updates, which is good for a large file system.
    const base::StringValue value(file_system_as_text);
    web_ui()->CallJavascriptFunction("updateFileSystemContents", value);
  }

  // Start updating the cache contents section once all directories are
  // processed.
  if (num_pending_reads_ == 0) {
    GetSystemService()->cache()->GetResourceIdsOfAllFilesOnUIThread(
        base::Bind(&DriveInternalsWebUIHandler::OnGetResourceIdsOfAllFiles,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void DriveInternalsWebUIHandler::OnGetResourceIdsOfAllFiles(
    const std::vector<std::string>& resource_ids) {
  for (size_t i = 0; i < resource_ids.size(); ++i) {
    const std::string& resource_id = resource_ids[i];
    GetSystemService()->cache()->GetCacheEntryOnUIThread(
        resource_id,
        "",  // Don't check MD5.
        base::Bind(&DriveInternalsWebUIHandler::OnGetCacheEntry,
                   weak_ptr_factory_.GetWeakPtr(),
                   resource_id));
  }
}

void DriveInternalsWebUIHandler::OnGetCacheEntry(
    const std::string& resource_id,
    bool success,
    const gdata::DriveCacheEntry& cache_entry) {
  if (!success) {
    LOG(ERROR) << "Failed to get cache entry: " << resource_id;
    return;
  }

  // Convert |cache_entry| into a dictionary.
  base::DictionaryValue value;
  value.SetString("resource_id", resource_id);
  value.SetString("md5", cache_entry.md5());
  value.SetBoolean("is_present", cache_entry.is_present());
  value.SetBoolean("is_pinned", cache_entry.is_pinned());
  value.SetBoolean("is_dirty", cache_entry.is_dirty());
  value.SetBoolean("is_mounted", cache_entry.is_mounted());
  value.SetBoolean("is_persistent", cache_entry.is_persistent());

  web_ui()->CallJavascriptFunction("updateCacheContents", value);
}

void DriveInternalsWebUIHandler::OnGetFreeDiskSpace(
    base::DictionaryValue* local_storage_summary) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_storage_summary);

  web_ui()->CallJavascriptFunction(
      "updateLocalStorageUsage", *local_storage_summary);
}

void DriveInternalsWebUIHandler::OnPeriodicUpdate(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata::DriveSystemService* system_service = GetSystemService();
  // |system_service| may be NULL in the guest/incognito mode.
  if (!system_service)
    return;

  gdata::DriveServiceInterface* drive_service = system_service->drive_service();
  DCHECK(drive_service);

  UpdateInFlightOperations(drive_service);
}

void DriveInternalsWebUIHandler::UpdateInFlightOperations(
    const gdata::DriveServiceInterface* drive_service) {
  std::vector<gdata::OperationRegistry::ProgressStatus>
      progress_status_list = drive_service->operation_registry()->
      GetProgressStatusList();

  base::ListValue in_flight_operations;
  for (size_t i = 0; i < progress_status_list.size(); ++i) {
    const gdata::OperationRegistry::ProgressStatus status =
        progress_status_list[i];

    base::DictionaryValue* dict = new DictionaryValue;
    dict->SetInteger("operation_id", status.operation_id);
    dict->SetString(
        "operation_type",
        gdata::OperationRegistry::OperationTypeToString(status.operation_type));
    dict->SetString("file_path", status.file_path.AsUTF8Unsafe());
    dict->SetString(
        "transfer_state",
        gdata::OperationRegistry::OperationTransferStateToString(
            status.transfer_state));
    dict->SetString(
        "start_time",
        gdata::util::FormatTimeAsStringLocaltime(status.start_time));
    dict->SetDouble("progress_current", status.progress_current);
    dict->SetDouble("progress_total", status.progress_total);
    in_flight_operations.Append(dict);
  }
  web_ui()->CallJavascriptFunction("updateInFlightOperations",
                                   in_flight_operations);
}

}  // namespace

DriveInternalsUI::DriveInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new DriveInternalsWebUIHandler());

  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIDriveInternalsHost);
  source->add_resource_path("drive_internals.css", IDR_DRIVE_INTERNALS_CSS);
  source->add_resource_path("drive_internals.js", IDR_DRIVE_INTERNALS_JS);
  source->set_default_resource(IDR_DRIVE_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

}  // namespace chromeos
