// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/captcha_view.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/password_changed_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/wm_message_listener.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "gfx/size.h"

namespace chromeos {

class Authenticator;
class BackgroundView;
class MessageBubble;

// ExistingUserController is used to handle login when someone has already
// logged into the machine. When Init is invoked a UserController is created for
// each of the Users's in the UserManager (including one for guest), and the
// window manager is then told to show the windows. If the user clicks on the
// guest entry the WizardWindow is swapped in.
//
// To use ExistingUserController create an instance of it and invoke Init.
//
// ExistingUserController maintains it's own life cycle and deletes itself when
// the user logs in (or chooses to see other settings).
class ExistingUserController : public WmMessageListener::Observer,
                               public UserController::Delegate,
                               public LoginStatusConsumer,
                               public InfoBubbleDelegate,
                               public CaptchaView::Delegate,
                               public PasswordChangedView::Delegate {
 public:
  // Initializes views for known users. |background_bounds| determines the
  // bounds of background view.
  ExistingUserController(const std::vector<UserManager::User>& users,
                         const gfx::Rect& background_bounds);

  // Creates and shows the appropriate set of windows.
  void Init();

  // Takes ownership of the specified background widget and view.
  void OwnBackground(views::Widget* background_widget,
                     chromeos::BackgroundView* background_view);

 private:
  friend class DeleteTask<ExistingUserController>;

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

  // LoginStatusConsumer:
  virtual void OnLoginFailure(const std::string& error);
  virtual void OnLoginSuccess(const std::string& username,
      const GaiaAuthConsumer::ClientLoginResult& credentials);
  virtual void OnOffTheRecordLoginSuccess();
  virtual void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Overridden from views::InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) {
    bubble_ = NULL;
  }
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }

  // CaptchaView::Delegate:
  virtual void OnCaptchaEntered(const std::string& captcha);

  // PasswordChangedView::Delegate:
  virtual void RecoverEncryptedData(const std::string& old_password);
  virtual void ResyncEncryptedData();

  // Adds start url to command line.
  void AppendStartUrlToCmdline();

  // Clears existing captcha state;
  void ClearCaptchaState();

  // Returns corresponding native window.
  gfx::NativeWindow GetNativeWindow() const;

  // Show error message. |error_id| error message ID in resources.
  // If |details| string is not empty, it specify additional error text
  // provided by authenticator, it is not localized.
  void ShowError(int error_id, const std::string& details);

  // Send message to window manager to enable/disable click on other windows.
  void SendSetLoginState(bool is_login);

  // Bounds of the background window.
  const gfx::Rect background_bounds_;

  // Background window/view.
  views::Widget* background_window_;
  BackgroundView* background_view_;

  // The set of UserControllers.
  std::vector<UserController*> controllers_;

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  // Index of selected view (user).
  size_t selected_view_index_;

  // See comment in ProcessWmMessage.
  base::OneShotTimer<ExistingUserController> delete_timer_;

  // Pointer to the instance that was scheduled to be deleted soon or NULL
  // if there is no such instance.
  static ExistingUserController* delete_scheduled_instance_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // Token representing the specific CAPTCHA challenge.
  std::string login_token_;

  // String entered by the user as an answer to a CAPTCHA challenge.
  std::string login_captcha_;

  // URL that will be opened on browser startup.
  GURL start_url_;

  // Cached credentials data when password change is detected.
  GaiaAuthConsumer::ClientLoginResult cached_credentials_;

  DISALLOW_COPY_AND_ASSIGN(ExistingUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
