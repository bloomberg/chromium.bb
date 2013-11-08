// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_CONTROLLER_H_

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Per-tab class to control the Omnibox password icon's visibility.
class ManagePasswordsIconController
    : public TabSpecificContentSettings::PasswordObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ManagePasswordsIconController> {
 public:
  virtual ~ManagePasswordsIconController();

  bool password_to_be_saved() const {
    return password_to_be_saved_;
  }

  bool manage_passwords_bubble_shown() const {
    return manage_passwords_bubble_shown_;
  }

  // Called when the bubble is opened after the icon gets displayed. We change
  // the state to know that we do not need to pop up the bubble again.
  void OnBubbleShown();

 private:
  friend class content::WebContentsUserData<ManagePasswordsIconController>;

  explicit ManagePasswordsIconController(content::WebContents* web_contents);

  // TabSpecificContentSettings::PasswordObserver:
  virtual void OnPasswordAction() OVERRIDE;

  bool password_to_be_saved_;
  bool manage_passwords_bubble_shown_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconController);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_CONTROLLER_H_
