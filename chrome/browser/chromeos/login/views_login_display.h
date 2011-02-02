// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_LOGIN_DISPLAY_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "gfx/rect.h"

namespace chromeos {

class HelpAppLauncher;
class MessageBubble;

// Views-based login UI implementation.
// Uses UserController for each user pod / guest pod / new user pod and
// ExistingUserView / GuestUserView / NewUserView as their views.
// When Init is invoked, a UserController is created for each of the Users's
// in the UserManager (including one for new user and one for Guest login),
// and the window manager is then told to show the windows.
class ViewsLoginDisplay : public LoginDisplay,
                          public UserController::Delegate,
                          public MessageBubbleDelegate {
 public:
  ViewsLoginDisplay(LoginDisplay::Delegate* delegate,
                    const gfx::Rect& background_bounds);
  virtual ~ViewsLoginDisplay();

  // LoginDisplay implementation:
  virtual void Init(const std::vector<UserManager::User>& users,
                    bool show_guest,
                    bool show_new_user);
  virtual void OnBeforeUserRemoved(const std::string& username);
  virtual void OnUserImageChanged(UserManager::User* user);
  virtual void OnUserRemoved(const std::string& username);
  virtual void SetUIEnabled(bool is_enabled);
  virtual void ShowError(int error_msg_id,
                         int login_attempts,
                         HelpAppLauncher::HelpTopic help_topic_id);

  // UserController::Delegate implementation:
  virtual void CreateAccount();
  virtual void Login(UserController* source, const string16& password);
  virtual void LoginAsGuest();
  virtual void ClearErrors();
  virtual void OnUserSelected(UserController* source);
  virtual void RemoveUser(UserController* source);
  virtual void SelectUser(int index);

  // Overridden from views::MessageBubbleDelegate:
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) { bubble_ = NULL; }
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }
  virtual void OnHelpLinkActivated();

 private:
  // Returns existing UserController instance by |email|.
  // NULL is returned if relevant instance is not found.
  UserController* GetUserControllerByEmail(const std::string& email);

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // UserController that corresponds user that's in process of being removed.
  // Has meaningful value only between OnBeforeUserRemoved()
  // and OnUserRemoved() calls.
  UserController* controller_for_removal_;

  // The set of visible UserControllers.
  std::vector<UserController*> controllers_;

  // Help application used for help dialogs.
  scoped_ptr<HelpAppLauncher> help_app_;

  // Last error help topic ID.
  HelpAppLauncher::HelpTopic help_topic_id_;

  // The set of invisible UserControllers.
  std::vector<UserController*> invisible_controllers_;

  // Index of selected user (from the set of visible users).
  size_t selected_view_index_;

  DISALLOW_COPY_AND_ASSIGN(ViewsLoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_LOGIN_DISPLAY_H_

