// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include "base/observer_list.h"

// Encapsulates the model behind the Web Authentication request dialog.
//
// Ultimately, this will become an observer of the AuthenticatorRequest, and
// contain the logic to figure out which steps the user needs to take, in which
// order, to complete the authentication flow.
class AuthenticatorRequestDialogModel {
 public:
  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer {
   public:
    // Called just before the model is destructed.
    virtual void OnModelDestroyed() = 0;

    // Called when the authentication request completes (successfully or not).
    virtual void OnRequestComplete() {}
  };

  AuthenticatorRequestDialogModel();
  ~AuthenticatorRequestDialogModel();

  // The |observer| must either outlive the object, or unregister itself on its
  // destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // To be called when the Web Authentication request is complete.
  void OnRequestComplete();

 private:
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
