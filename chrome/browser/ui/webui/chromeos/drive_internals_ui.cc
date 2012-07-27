// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_auth_service.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

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
                       base::ListValue* gcache_contents) {
  using file_util::FileEnumerator;
  // Use this map to sort the result list by the path.
  std::map<FilePath, DictionaryValue*> files;

  const int options = (file_util::FileEnumerator::FILES |
                       file_util::FileEnumerator::DIRECTORIES |
                       file_util::FileEnumerator::SHOW_SYM_LINKS);
  FileEnumerator enumerator(
      root_path,
      true,  // recursive
      static_cast<FileEnumerator::FileType>(options));

  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    FileEnumerator::FindInfo find_info;
    enumerator.GetFindInfo(&find_info);
    int64 size = FileEnumerator::GetFilesize(find_info);
    const bool is_directory = FileEnumerator::IsDirectory(find_info);
    const bool is_symbolic_link = FileEnumerator::IsLink(find_info);
    const base::Time last_modified =
        FileEnumerator::GetLastModifiedTime(find_info);

    base::DictionaryValue* entry = new base::DictionaryValue;
    entry->SetString("path", current.value());
    // Use double instead of integer for large files.
    entry->SetDouble("size", size);
    entry->SetBoolean("is_directory", is_directory);
    entry->SetBoolean("is_symbolic_link", is_symbolic_link);
    entry->SetString("last_modified",
                     gdata::util::FormatTimeAsString(last_modified));

    files[current] = entry;
  }

  // Convert |files| into |gcache_contents|.
  for (std::map<FilePath, DictionaryValue*>::const_iterator
           iter = files.begin(); iter != files.end(); ++iter) {
    gcache_contents->Append(iter->second);
  }
}

// Class to handle messages from chrome://drive-internals.
class DriveInternalsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DriveInternalsWebUIHandler()
      : weak_ptr_factory_(this) {
  }

  virtual ~DriveInternalsWebUIHandler() {
  }

 private:
  // WebUIMessageHandler override.
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback(
        "pageLoaded",
        base::Bind(&DriveInternalsWebUIHandler::OnPageLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when the page is first loaded.
  void OnPageLoaded(const base::ListValue* args) {
    Profile* profile = Profile::FromWebUI(web_ui());
    gdata::GDataSystemService* system_service =
        gdata::GDataSystemServiceFactory::GetForProfile(profile);
    // |system_service| may be NULL in the guest/incognito mode.
    if (!system_service)
      return;

    gdata::DocumentsServiceInterface* documents_service =
        system_service->docs_service();
    DCHECK(documents_service);

    // Update the auth status section.
    base::DictionaryValue auth_status;
    auth_status.SetBoolean("has-refresh-token",
                           documents_service->HasRefreshToken());
    auth_status.SetBoolean("has-access-token",
                           documents_service->HasAccessToken());
    web_ui()->CallJavascriptFunction("updateAuthStatus", auth_status);

    // Start updating the GCache contents section.
    const FilePath root_path =
        gdata::GDataCache::GetCacheRootPath(profile);
    base::ListValue* gcache_contents = new ListValue;
    content::BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&GetGCacheContents, root_path, gcache_contents),
        base::Bind(&DriveInternalsWebUIHandler::OnGetGCacheContents,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(gcache_contents)));
  }

  // Called when GetGCacheContents() is complete.
  void OnGetGCacheContents(base::ListValue* gcache_contents) {
    DCHECK(gcache_contents);
    web_ui()->CallJavascriptFunction("updateGCacheContents", *gcache_contents);
  }

  base::WeakPtrFactory<DriveInternalsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveInternalsWebUIHandler);
};

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
