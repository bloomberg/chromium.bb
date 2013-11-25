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
      manage_passwords_icon_to_be_shown_(false),
      password_to_be_saved_(false),
      manage_passwords_bubble_needs_showing_(false),
      browser_context_(web_contents->GetBrowserContext()) {}

ManagePasswordsIconController::~ManagePasswordsIconController() {}

void ManagePasswordsIconController::OnBubbleShown() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  DCHECK(content_settings);
  content_settings->unset_manage_passwords_bubble_needs_showing();
}

void ManagePasswordsIconController::OnPasswordAction() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  DCHECK(content_settings);
  manage_passwords_icon_to_be_shown_ =
      content_settings->manage_passwords_icon_to_be_shown();
  password_to_be_saved_ = content_settings->password_to_be_saved();
  manage_passwords_bubble_needs_showing_ =
      content_settings->manage_passwords_bubble_needs_showing();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  LocationBar* location_bar = browser->window()->GetLocationBar();
  DCHECK(location_bar);
  location_bar->UpdateManagePasswordsIconAndBubble();
}
