// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_task_executor.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

namespace drive {

using file_handler_util::FileTaskExecutor;

DriveTaskExecutor::DriveTaskExecutor(Profile* profile,
                                     const std::string& app_id,
                                     const std::string& action_id)
  : file_handler_util::FileTaskExecutor(profile, GURL(), "", app_id),
    action_id_(action_id),
    current_index_(0) {
  DCHECK("open-with" == action_id_);
}

DriveTaskExecutor::~DriveTaskExecutor() {
}

bool DriveTaskExecutor::ExecuteAndNotify(
    const std::vector<GURL>& file_urls,
    const file_handler_util::FileTaskFinishedCallback& done) {
  std::vector<FilePath> raw_paths;
  for (std::vector<GURL>::const_iterator iter = file_urls.begin();
      iter != file_urls.end(); ++iter) {
    fileapi::FileSystemURL url(*iter);
    if (!url.is_valid() || url.type() != fileapi::kFileSystemTypeDrive)
      return false;
    raw_paths.push_back(url.virtual_path());
  }

  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile());
  DCHECK(current_index_ == 0);
  if (!system_service || !system_service->file_system())
    return false;
  DriveFileSystemInterface* file_system = system_service->file_system();

  // Reset the index, so we know when we're done.
  current_index_ = raw_paths.size();

  for (std::vector<FilePath>::const_iterator iter = raw_paths.begin();
      iter != raw_paths.end(); ++iter) {
    file_system->GetEntryInfoByPath(
        *iter,
        base::Bind(&DriveTaskExecutor::OnFileEntryFetched, this));
  }
  return true;
}

void DriveTaskExecutor::OnFileEntryFetched(
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  // If we aborted, then this will be zero.
  if (!current_index_)
    return;

  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile());

  // Here, we are only interested in files.
  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  if (!system_service || error != DRIVE_FILE_OK) {
    Done(false);
    return;
  }

  // The edit URL can be empty for non-editable files (such as files shared with
  // read-only privilege).
  if (entry_proto->edit_url().empty()) {
    Done(false);
    return;
  }

  google_apis::DriveServiceInterface* drive_service =
      system_service->drive_service();

  // Send off a request for the drive service to authorize the apps for the
  // current document entry for this document so we can get the
  // open-with-<app_id> urls from the document entry.
  drive_service->AuthorizeApp(
      GURL(entry_proto->edit_url()),
      extension_id(),  // really app_id
      base::Bind(&DriveTaskExecutor::OnAppAuthorized,
                 this,
                 entry_proto->resource_id()));
}

void DriveTaskExecutor::OnAppAuthorized(
    const std::string& resource_id,
    google_apis::GDataErrorCode error,
    const GURL& open_link) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // If we aborted, then this will be zero.
  if (!current_index_)
    return;

  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile());

  if (!system_service || error != google_apis::HTTP_SUCCESS) {
    Done(false);
    return;
  }

  if (open_link.is_empty()) {
    Done(false);
    return;
  }

  Browser* browser = GetBrowser();
  chrome::AddSelectedTabWithURL(browser, open_link,
                                content::PAGE_TRANSITION_LINK);
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

}  // namespace drive
