// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_PERMISSION_REQUEST_H_

#include <string>

#include "base/ref_counted.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "chrome/common/content_settings.h"
#include "googleurl/src/gurl.h"

// This class is used to request content setting related permission for local
// storage.  It should only be used for one such event and then discarded.
class DOMStoragePermissionRequest : public CookiePromptModalDialogDelegate {
 public:
  DOMStoragePermissionRequest(const GURL& url,
                              const string16& key,
                              const string16& value,
                              HostContentSettingsMap* settings);


  ContentSetting WaitOnResponse();

  const GURL& url() const { return url_; }
  const string16& key() const { return key_; }
  const string16& value() const { return value_; }

  // Called on the UI thread.
  static void PromptUser(DOMStoragePermissionRequest* request);

  // CookiesPromptViewDelegate methods:
  virtual void AllowSiteData(bool session_expire);
  virtual void BlockSiteData();

 private:
  void SendResponse(ContentSetting content_setting);

  // The URL we need to get permission for.
  const GURL url_;

  // The key we're trying to set.
  const string16 key_;

  // The value we're trying to set.
  const string16 value_;

  // The response to the permission request.
  ContentSetting response_content_setting_;

  // One time use.  Never reset.
  base::WaitableEvent event_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStoragePermissionRequest);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_PERMISSION_REQUEST_H_
