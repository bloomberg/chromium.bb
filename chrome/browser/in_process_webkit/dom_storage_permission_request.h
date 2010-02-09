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
                              bool file_exists,
                              int64 size,
                              base::Time last_modified,
                              HostContentSettingsMap* settings);


  ContentSetting WaitOnResponse();
  void SendResponse(ContentSetting content_setting, bool remember);

  const GURL& url() const { return url_; }
  bool file_exists() const { return file_exists_; }
  int64 size() const { return size_; }
  const base::Time last_modified() const { return last_modified_; }

  // Called on the UI thread.
  static void PromptUser(DOMStoragePermissionRequest* request);

  // CookiesPromptViewDelegate methods:
  virtual void AllowSiteData(bool remember, bool session_expire);
  virtual void BlockSiteData(bool remember);

 private:
  // The URL we need to get permission for.
  const GURL url_;

  // Is there any information on disk currently?
  bool file_exists_;

  // If file_exists_, what's the size?
  int64 size_;

  // If file_exists_, what's the size?
  const base::Time last_modified_;

  // The response to the permission request.
  ContentSetting response_content_setting_;

  // One time use.  Never reset.
  base::WaitableEvent event_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStoragePermissionRequest);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_PERMISSION_REQUEST_H_
