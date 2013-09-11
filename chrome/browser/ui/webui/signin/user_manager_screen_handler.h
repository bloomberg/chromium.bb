// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

class UserManagerScreenHandler : public content::WebUIMessageHandler {
 public:
  UserManagerScreenHandler();
  virtual ~UserManagerScreenHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

 private:
  // An observer for any changes to Profiles in the ProfileInfoCache so that
  // all the visible user manager screens can be updated.
  class ProfileUpdateObserver;

  void HandleInitialize(const base::ListValue* args);
  void HandleAddUser(const base::ListValue* args);
  void HandleLaunchGuest(const base::ListValue* args);
  void HandleLaunchUser(const base::ListValue* args);
  void HandleRemoveUser(const base::ListValue* args);

  // Sends user list to account chooser.
  void SendUserList();

  // Observes the ProfileInfoCache and gets notified when a profile has been
  // modified, so that the displayed user pods can be updated.
  scoped_ptr<ProfileUpdateObserver> profileInfoCacheObserver_;

  // The host desktop type this user manager belongs to.
  chrome::HostDesktopType desktop_type_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerScreenHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_
