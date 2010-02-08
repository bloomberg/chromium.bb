// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_permission_request.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/message_box_handler.h"

DOMStoragePermissionRequest::DOMStoragePermissionRequest(
    const std::string& host,
    bool file_exists,
    int64 size,
    base::Time last_modified,
    HostContentSettingsMap* settings)
    : host_(host),
      file_exists_(file_exists),
      size_(size),
      last_modified_(last_modified),
      event_(true, false),  // manual reset, not initially signaled
      host_content_settings_map_(settings) {
}

ContentSetting DOMStoragePermissionRequest::WaitOnResponse() {
  event_.Wait();
  return response_content_setting_;
}

void DOMStoragePermissionRequest::SendResponse(ContentSetting content_setting,
                                               bool remember) {
  response_content_setting_ = content_setting;
  if (remember) {
    host_content_settings_map_->SetContentSetting(
        host_, CONTENT_SETTINGS_TYPE_COOKIES, content_setting);
  }
  event_.Signal();
}

// static
void DOMStoragePermissionRequest::PromptUser(
    DOMStoragePermissionRequest *dom_storage_permission_request) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Cookie settings may have changed.
  ContentSetting setting =
      dom_storage_permission_request->host_content_settings_map_->
          GetContentSetting(dom_storage_permission_request->host(),
                            CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting != CONTENT_SETTING_ASK) {
    dom_storage_permission_request->SendResponse(setting, false);
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->GetSelectedTabContents()) {
    dom_storage_permission_request->SendResponse(CONTENT_SETTING_BLOCK, false);
    return;
  }

#if defined(OS_WIN)
  // TODO(darin): It seems like it would be interesting if the dialog actually
  // showed the name and value being stored (as is done for cookies).
  RunLocalStoragePrompt(browser->GetSelectedTabContents(),
                        BrowsingDataLocalStorageHelper::LocalStorageInfo(
                            std::string(),
                            dom_storage_permission_request->host(),
                            -1,
                            std::string(),
                            dom_storage_permission_request->host(),
                            FilePath(),
                            dom_storage_permission_request->size(),
                            dom_storage_permission_request->last_modified()),
                        dom_storage_permission_request);
#else
  // TODO(darin): Enable prompting for other ports.
  dom_storage_permission_request->SendResponse(CONTENT_SETTING_BLOCK, false);
#endif
}

void DOMStoragePermissionRequest::AllowSiteData(bool remember,
                                                bool session_expire) {
  // The session_expire parameter is not relevant.
  SendResponse(CONTENT_SETTING_ALLOW, remember);
}

void DOMStoragePermissionRequest::BlockSiteData(bool remember) {
  SendResponse(CONTENT_SETTING_BLOCK, remember);
}
