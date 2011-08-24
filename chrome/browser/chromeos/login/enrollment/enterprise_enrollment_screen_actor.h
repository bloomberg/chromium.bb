// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"

class GoogleServiceAuthError;

namespace chromeos {

// Interface class for the enterprise enrollment screen actor.
class EnterpriseEnrollmentScreenActor {
 public:
  // This defines the interface for controllers which will be called back when
  // something happens on the UI.
  class Controller {
   public:
    virtual ~Controller() {}

    virtual void OnAuthSubmitted(const std::string& user,
                                 const std::string& password,
                                 const std::string& captcha,
                                 const std::string& access_code) = 0;
    virtual void OnOAuthTokenAvailable(const std::string& user,
                                       const std::string& oauth_token) = 0;
    virtual void OnAuthCancelled() = 0;
    virtual void OnConfirmationClosed() = 0;
    virtual bool GetInitialUser(std::string* user) = 0;
  };

  // Used in PyAuto testing.
  class Observer {
   public:
    virtual ~Observer() {}

    // Notifies observers of a change in enrollment state.
    virtual void OnEnrollmentComplete(
        EnterpriseEnrollmentScreenActor* enrollment_screen,
        bool succeeded) = 0;
  };

  EnterpriseEnrollmentScreenActor();

  virtual ~EnterpriseEnrollmentScreenActor();

  void AddObserver(Observer* obs);

  void RemoveObserver(Observer* obs);

  // Sets the controller.
  virtual void SetController(Controller* controller) = 0;

  // Prepare the contents to showing.
  virtual void PrepareToShow() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets whether the user is editable.
  virtual void SetEditableUser(bool editable) = 0;

  // Switches to the confirmation screen.
  virtual void ShowConfirmationScreen() = 0;

  // Show an authentication error.
  virtual void ShowAuthError(const GoogleServiceAuthError& error) = 0;
  virtual void ShowAccountError() = 0;
  virtual void ShowFatalAuthError() = 0;
  virtual void ShowFatalEnrollmentError() = 0;
  virtual void ShowNetworkEnrollmentError() = 0;

 protected:
  void NotifyObservers(bool succeeded);

 private:
  // Observers.
  ObserverList<Observer> observer_list_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
