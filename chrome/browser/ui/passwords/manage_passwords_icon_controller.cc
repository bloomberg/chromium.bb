// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_icon_controller.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "content/public/browser/notification_service.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsIconController);

ManagePasswordsIconController::ManagePasswordsIconController(
    content::WebContents* web_contents)
    : TabSpecificContentSettings::PasswordObserver(
          TabSpecificContentSettings::FromWebContents(web_contents)),
      content::WebContentsObserver(web_contents),
      password_to_be_saved_(false),
      manage_passwords_bubble_shown_(false),
      browser_context_(web_contents->GetBrowserContext()) {}

ManagePasswordsIconController::~ManagePasswordsIconController() {}

void ManagePasswordsIconController::OnBubbleShown() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  DCHECK(content_settings);
  content_settings->set_manage_passwords_bubble_shown();
}

void ManagePasswordsIconController::OnPasswordAction() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  DCHECK(content_settings);
  password_to_be_saved_ = content_settings->password_to_be_saved();
  manage_passwords_bubble_shown_ =
      content_settings->manage_passwords_bubble_shown();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  LocationBar* location_bar = browser->window()->GetLocationBar();
  DCHECK(location_bar);
  location_bar->UpdateManagePasswordsIconAndBubble();
}
