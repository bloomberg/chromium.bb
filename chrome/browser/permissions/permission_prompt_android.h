// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_ANDROID_H_

#include "chrome/browser/ui/website_settings/permission_prompt.h"

class InfoBarService;

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
}

class PermissionPromptAndroid : public PermissionPrompt {
 public:
  explicit PermissionPromptAndroid(content::WebContents* web_contents);
  ~PermissionPromptAndroid() override;

  // PermissionPrompt:
  void SetDelegate(Delegate* delegate) override;
  void Show(const std::vector<PermissionRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  bool CanAcceptRequestUpdate() override;
  void Hide() override;
  bool IsVisible() override;
  void UpdateAnchorPosition() override;
  gfx::NativeWindow GetNativeWindow() override;

  void Closing();
  void ToggleAccept(int index, bool value);
  void Accept();
  void Deny();

 private:
  // PermissionPromptAndroid is owned by PermissionRequestManager, so it should
  // be safe to hold a raw WebContents pointer here because this class is
  // destroyed before the WebContents.
  content::WebContents* web_contents_;
  // |delegate_| is the PermissionRequestManager, which owns this object.
  Delegate* delegate_;
  // |infobar_| is owned by the InfoBarService; we keep a pointer here so we can
  // ask the service to remove the infobar after it is added.
  infobars::InfoBar* infobar_;

  DISALLOW_COPY_AND_ASSIGN(PermissionPromptAndroid);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_ANDROID_H_
