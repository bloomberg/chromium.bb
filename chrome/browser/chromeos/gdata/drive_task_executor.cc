// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_task_executor.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

namespace gdata {

using file_handler_util::FileTaskExecutor;

DriveTaskExecutor::DriveTaskExecutor(Profile* profile,
                                     const std::string& app_id,
                                     const std::string& action_id)
  : file_handler_util::FileTaskExecutor(profile),
    app_id_(app_id),
    action_id_(action_id),
    current_index_(0) {
  DCHECK("open-with" == action_id_);
  DCHECK(app_id.size() > FileTaskExecutor::kDriveTaskExtensionPrefixLength);
  DCHECK(StartsWithASCII(app_id,
                         FileTaskExecutor::kDriveTaskExtensionPrefix,
                         false));
  // Strip off the prefix from the extension ID so we convert it to an app id.
  app_id_ = app_id_.substr(FileTaskExecutor::kDriveTaskExtensionPrefixLength);
}

DriveTaskExecutor::~DriveTaskExecutor() {
}

bool DriveTaskExecutor::ExecuteAndNotify(
    const std::vector<GURL>& file_urls,
    const file_handler_util::FileTaskFinishedCallback& done) {
  std::vector<FilePath> raw_paths;
  for (std::vector<GURL>::const_iterator iter = file_urls.begin();
      iter != file_urls.end(); ++iter) {
    FilePath raw_path;
    fileapi::FileSystemType type = fileapi::kFileSystemTypeUnknown;
    if (!fileapi::CrackFileSystemURL(*iter, NULL, &type, &raw_path) ||
        type != fileapi::kFileSystemTypeExternal) {
      return false;
    }
    raw_paths.push_back(raw_path);
  }

  gdata::GDataSystemService* system_service =
      gdata::GDataSystemServiceFactory::GetForProfile(profile());
  DCHECK(current_index_ == 0);
  if (!system_service || !system_service->file_system())
    return false;
  gdata::GDataFileSystem* file_system = system_service->file_system();

  // Reset the index, so we know when we're done.
  current_index_ = raw_paths.size();

  for (std::vector<FilePath>::const_iterator iter = raw_paths.begin();
      iter != raw_paths.end(); ++iter) {
    file_system->GetFileInfoByPath(
        *iter,
        base::Bind(&DriveTaskExecutor::OnFileEntryFetched, this));
  }
  return true;
}

void DriveTaskExecutor::OnFileEntryFetched(
    base::PlatformFileError error,
    scoped_ptr<gdata::GDataFileProto> file_proto) {
  // If we aborted, then this will be zero.
  if (!current_index_)
    return;

  gdata::GDataSystemService* system_service =
      gdata::GDataSystemServiceFactory::GetForProfile(profile());

  if (!system_service || error != base::PLATFORM_FILE_OK) {
    Done(false);
    return;
  }

  gdata::DocumentsServiceInterface* docs_service =
      system_service->docs_service();

  // Send off a request for the document service to authorize the apps for the
  // current document entry for this document so we can get the
  // open-with-<app_id> urls from the document entry.
  docs_service->AuthorizeApp(
      GURL(file_proto->gdata_entry().edit_url()),
      app_id_,
      base::Bind(&DriveTaskExecutor::OnAppAuthorized,
                 this,
                 file_proto->gdata_entry().resource_id()));
}

void DriveTaskExecutor::OnAppAuthorized(
    const std::string& resource_id,
    gdata::GDataErrorCode error,
    scoped_ptr<base::Value> feed_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // If we aborted, then this will be zero.
  if (!current_index_)
    return;

  gdata::GDataSystemService* system_service =
      gdata::GDataSystemServiceFactory::GetForProfile(profile());

  if (!system_service || error != gdata::HTTP_SUCCESS) {
    Done(false);
    return;
  }

  // Yay!  We've got the feed data finally, and we can get the open-with URL.
  GURL open_with_url;
  base::ListValue* link_list = NULL;
  feed_data->GetAsList(&link_list);
  for (size_t i = 0; i < link_list->GetSize(); ++i) {
    DictionaryValue* entry = NULL;
    link_list->GetDictionary(i, &entry);
    std::string app_id;
    entry->GetString("app_id", &app_id);
    if (app_id == app_id_) {
      std::string href;
      entry->GetString("href", &href);
      open_with_url = GURL(href);
      break;
    }
  }

  if (open_with_url.is_empty()) {
    Done(false);
    return;
  }

  Browser* browser = GetBrowser();
  browser->AddSelectedTabWithURL(open_with_url, content::PAGE_TRANSITION_LINK);
  // If the current browser is not tabbed then the new tab will be created
  // in a different browser. Make sure it is visible.
  browser->window()->Show();

  // We're done with this file.  If this is the last one, then we're done.
  current_index_--;
  DCHECK(current_index_ >= 0);
  if (current_index_ == 0)
    Done(true);
}

void DriveTaskExecutor::Done(bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  current_index_ = 0;
  if (!done_.is_null())
    done_.Run(success);
  done_.Reset();
}

}  // namespace gdata
