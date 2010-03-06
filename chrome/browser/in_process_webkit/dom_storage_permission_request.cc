// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_permission_request.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/message_box_handler.h"

DOMStoragePermissionRequest::DOMStoragePermissionRequest(
    const GURL& url,
    const string16& key,
    const string16& value,
    HostContentSettingsMap* settings)
    : url_(url),
      key_(key),
      value_(value),
      event_(true, false),  // manual reset, not initially signaled
      host_content_settings_map_(settings) {
}

ContentSetting DOMStoragePermissionRequest::WaitOnResponse() {
  event_.Wait();
  return response_content_setting_;
}

// static
void DOMStoragePermissionRequest::PromptUser(
    DOMStoragePermissionRequest* request) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Cookie settings may have changed.
  ContentSetting setting =
      request->host_content_settings_map_->GetContentSetting(
          request->url_, CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting != CONTENT_SETTING_ASK) {
    request->SendResponse(setting);
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->GetSelectedTabContents()) {
    request->SendResponse(CONTENT_SETTING_BLOCK);
    return;
  }

  RunLocalStoragePrompt(browser->GetSelectedTabContents(),
                        request->host_content_settings_map_, request->url_,
                        request->key_, request->value_, request);
}

void DOMStoragePermissionRequest::AllowSiteData(bool session_expire) {
  SendResponse(CONTENT_SETTING_ALLOW);
}

void DOMStoragePermissionRequest::BlockSiteData() {
  SendResponse(CONTENT_SETTING_BLOCK);
}

void DOMStoragePermissionRequest::SendResponse(
    ContentSetting content_setting) {
  response_content_setting_ = content_setting;
  event_.Signal();
}
