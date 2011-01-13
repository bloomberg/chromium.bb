// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/captcha_view.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/password_changed_view.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/wm_message_listener.h"
#include "gfx/size.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace chromeos {

class HelpAppLauncher;
class MessageBubble;
class UserCrosSettingsProvider;

// ExistingUserController is used to handle login when someone has
// already logged into the machine. When Init is invoked, a
// UserController is created for each of the Users's in the
// UserManager (including one for new user and one for Guest login),
// and the window manager is then told to show the windows.
//
// To use ExistingUserController create an instance of it and invoke Init.
//
// ExistingUserController maintains it's own life cycle and deletes itself when
// the user logs in (or chooses to see other settings).
class ExistingUserController : public WmMessageListener::Observer,
                               public UserController::Delegate,
                               public LoginPerformer::Delegate,
                               public MessageBubbleDelegate,
                               public CaptchaView::Delegate,
                               public PasswordChangedView::Delegate {
 public:
  // Initializes views for known users. |background_bounds| determines the
  // bounds of background view.
  ExistingUserController(const std::vector<UserManager::User>& users,
                         const gfx::Rect& background_bounds);

  // Returns the current existing user controller if it has been created.
  static ExistingUserController* current_controller() {
    return current_controller_;
  }

  // Creates and shows the appropriate set of windows.
  void Init();

  // Takes ownership of the specified background widget and view.
  void OwnBackground(views::Widget* background_widget,
                     chromeos::BackgroundView* background_view);

  // Tries to login from new user pod with given user login and password.
  // Called after creating new account.
  void LoginNewUser(const std::string& username, const std::string& password);

  // Selects new user pod.
  void SelectNewUser();

 private:
  friend class DeleteTask<ExistingUserController>;
  friend class MockLoginPerformerDelegate;

  ~ExistingUserController();

  // Cover for invoking the destructor. Used by delete_timer_.
  void Delete();

  // WmMessageListener::Observer:
  virtual void ProcessWmMessage(const WmIpc::Message& message,
                                GdkWindow* window);

  // UserController::Delegate:
  virtual void Login(UserController* source, const string16& password);
  virtual void LoginOffTheRecord();
  virtual void ClearErrors();
  virtual void OnUserSelected(UserController* source);
  virtual void ActivateWizard(const std::string& screen_name);
  virtual void RemoveUser(UserController* source);
  virtual void AddStartUrl(const GURL& start_url) { start_url_ = start_url; }
  virtual void SelectUser(int index);
  virtual void SetStatusAreaEnabled(bool enable);

  // LoginPerformer::Delegate implementation:
  virtual void OnLoginFailure(const LoginFailure& error);
  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests);
  virtual void OnOffTheRecordLoginSuccess();
  virtual void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials);
  virtual void WhiteListCheckFailed(const std::string& email);

  // Overridden from views::InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) {
    bubble_ = NULL;
  }
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }
  virtual void OnHelpLinkActivated();

  // CaptchaView::Delegate:
  virtual void OnCaptchaEntered(const std::string& captcha);

  // PasswordChangedView::Delegate:
  virtual void RecoverEncryptedData(const std::string& old_password);
  virtual void ResyncEncryptedData();

  // Adds start url to command line.
  void AppendStartUrlToCmdline();

  // Returns corresponding native window.
  gfx::NativeWindow GetNativeWindow() const;

  // Show error message. |error_id| error message ID in resources.
  // If |details| string is not empty, it specify additional error text
  // provided by authenticator, it is not localized.
  void ShowError(int error_id, const std::string& details);

  // Send message to window manager to enable/disable click on other windows.
  void SendSetLoginState(bool is_login);

  void set_login_performer_delegate(LoginPerformer::Delegate* d) {
    login_performer_delegate_.reset(d);
  }

  // Bounds of the background window.
  const gfx::Rect background_bounds_;

  // Background window/view.
  views::Widget* background_window_;
  BackgroundView* background_view_;

  // The set of visible UserControllers.
  std::vector<UserController*> controllers_;

  // The set of invisible UserControllers.
  std::vector<UserController*> invisible_controllers_;

  // Used to execute login operations.
  scoped_ptr<LoginPerformer> login_performer_;

  // Delegate for login performer to be overridden by tests.
  // |this| is used if |login_performer_delegate_| is NULL.
  scoped_ptr<LoginPerformer::Delegate> login_performer_delegate_;

  // Index of selected view (user).
  size_t selected_view_index_;

  // Number of login attempts. Used to show help link when > 1 unsuccessful
  // logins for the same user.
  size_t num_login_attempts_;

  // See comment in ProcessWmMessage.
  base::OneShotTimer<ExistingUserController> delete_timer_;

  // Pointer to the current instance of the controller to be used by
  // automation tests.
  static ExistingUserController* current_controller_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // URL that will be opened on browser startup.
  GURL start_url_;

  // Help application used for help dialogs.
  scoped_ptr<HelpAppLauncher> help_app_;

  // Triggers prefetching of user settings.
  scoped_ptr<UserCrosSettingsProvider> user_settings_;

  // Factory of callbacks.
  ScopedRunnableMethodFactory<ExistingUserController> method_factory_;

  FRIEND_TEST_ALL_PREFIXES(ExistingUserControllerTest, NewUserLogin);

  DISALLOW_COPY_AND_ASSIGN(ExistingUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
