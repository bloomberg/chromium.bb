// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_HANDLER_ANDROID_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_HANDLER_ANDROID_H_

#include "base/scoped_observer.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Handler class for chrome://welcome page.
class WelcomeHandler : public content::WebUIMessageHandler,
                       public ProfileSyncServiceObserver {
 public:
  WelcomeHandler();
  virtual ~WelcomeHandler();

  // content::WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "updateSyncFooterVisibility" message. This makes the sync
  // footer in the page visible if sync is enabled.
  void HandleUpdateSyncFooterVisibility(const base::ListValue* args);

  // Callback for the "showSyncSettings" message. This opens the sync settings
  // menu fragment.
  void HandleShowSyncSettings(const base::ListValue* args);

  // Callback for the "showTermsOfService" message. This opens the terms of
  // service popup.
  void HandleShowTermsOfService(const base::ListValue* args);

  // ProfileSyncServiceObserver implementation
  virtual void OnStateChanged() OVERRIDE;

 private:
  // Update the sync footer visibility. Set |forced| if you want to force to
  // send an update to the renderer regardless of whether we assume the state
  // is up to date or not.
  void UpdateSyncFooterVisibility(bool forced);

  // Cached pointer to the sync service for this profile.
  ProfileSyncService* sync_service_;

  ScopedObserver<ProfileSyncService, WelcomeHandler> observer_manager_;

  // Whether the sync footer is visible on the page.
  bool is_sync_footer_visible_;

  DISALLOW_COPY_AND_ASSIGN(WelcomeHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_HANDLER_ANDROID_H_
