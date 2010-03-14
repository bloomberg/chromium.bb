// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/wm_message_listener.h"
#include "gfx/size.h"

class Authenticator;

namespace views {
class Wiget;
}

namespace chromeos {

class BackgroundView;

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
                               public LoginStatusConsumer {
 public:
  // |login_size| is the size of the login window that is passed to the
  // WizardWindow. |background_size| is not used if the user logs into using an
  // existing user.
  ExistingUserController(const std::vector<UserManager::User>& users,
                         const gfx::Size& background_size);

  // Creates and shows the appropriate set of windows.
  void Init();

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

  // LoginStatusConsumer:
  virtual void OnLoginFailure(const std::string error);
  virtual void OnLoginSuccess(const std::string username);

  // Size of the background window.
  const gfx::Size background_size_;

  // Background window/view.
  views::Widget* background_window_;
  BackgroundView* background_view_;

  // The set of UserControllers.
  std::vector<UserController*> controllers_;

  // Used for logging in.
  scoped_ptr<Authenticator> authenticator_;

  // Index of view loggin in.
  size_t index_of_view_logging_in_;

  // See comment in ProcessWmMessage.
  base::OneShotTimer<ExistingUserController> delete_timer_;

  DISALLOW_COPY_AND_ASSIGN(ExistingUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
