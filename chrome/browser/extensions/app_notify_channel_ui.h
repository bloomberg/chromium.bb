// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_H_

#include <string>

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// An interface for prompting a user to sign in to sync so that we can create
// an app notification channel for one of their apps.
class AppNotifyChannelUI {
 public:
  virtual ~AppNotifyChannelUI() {}

  // Used to customize the UI we show.
  enum UIType {
    // Do not prompt the user with an infobar.
    NO_INFOBAR,

    // Ask if the app can show notifications.
    NOTIFICATION_INFOBAR,
  };

  class Delegate {
   public:
    // A callback for whether the user successfully set up sync or not.
    virtual void OnSyncSetupResult(bool enabled) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Shows a prompt for sync setup - |delegate| will be called back later when
  // setup is complete or cancelled. This should only be called once per
  // instance.
  virtual void PromptSyncSetup(Delegate* delegate) = 0;

  // Builds the platform specific AppNotifyChannelUI.
  static AppNotifyChannelUI* Create(Profile* profile,
                                    content::WebContents* web_contents,
                                    const std::string& app_name,
                                    AppNotifyChannelUI::UIType ui_type);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_H_
