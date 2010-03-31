// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/database_permission_request.h"


#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/message_box_handler.h"

DatabasePermissionRequest::DatabasePermissionRequest(
    const GURL& url,
    const string16& database_name,
    const string16& display_name,
    unsigned long estimated_size,
    Task* on_allow,
    Task* on_block,
    HostContentSettingsMap* settings_map)
    : url_(url),
      database_name_(database_name),
      display_name_(display_name),
      estimated_size_(estimated_size),
      on_allow_(on_allow),
      on_block_(on_block),
      host_content_settings_map_(settings_map) {
  DCHECK(on_allow_.get());
  DCHECK(on_block_.get());
}

DatabasePermissionRequest::~DatabasePermissionRequest() {
}

void DatabasePermissionRequest::RequestPermission() {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE, NewRunnableMethod(
            this, &DatabasePermissionRequest::RequestPermission));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Cookie settings may have changed.
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      url_, CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting != CONTENT_SETTING_ASK) {
    SendResponse(setting);
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->GetSelectedTabContents()) {
    BlockSiteData();
    return;
  }

  self_ref_ = this;
  // Will call either AllowSiteData or BlockSiteData which will NULL out our
  // self reference.
  RunDatabasePrompt(browser->GetSelectedTabContents(),
                    host_content_settings_map_, url_, database_name_,
                    display_name_, estimated_size_, this);
}

void DatabasePermissionRequest::AllowSiteData(bool session_expire) {
  SendResponse(CONTENT_SETTING_ALLOW);
}

void DatabasePermissionRequest::BlockSiteData() {
  SendResponse(CONTENT_SETTING_BLOCK);
}

void DatabasePermissionRequest::SendResponse(ContentSetting content_setting) {
  if (content_setting == CONTENT_SETTING_ALLOW) {
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, on_allow_.release());
  } else {
    DCHECK(content_setting == CONTENT_SETTING_BLOCK);
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, on_block_.release());
  }

  // Release all resources.
  on_allow_.reset();
  on_block_.reset();

  // This seems safer than possibly being deleted while in method(s) related to
  // this object.  Any thread will do, but UI is always around and can be
  // posted without locking, so we'll ask it to do the release.
  ChromeThread::ReleaseSoon(ChromeThread::UI, FROM_HERE, self_ref_.release());
}
