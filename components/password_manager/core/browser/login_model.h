// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_MODEL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_MODEL_H_

#include "base/strings/string16.h"

namespace password_manager {

// Simple Model & Observer interfaces for a LoginView to facilitate exchanging
// information.
class LoginModelObserver {
 public:
  // Called by the model when a username,password pair has been identified
  // as a match for the pending login prompt.
  virtual void OnAutofillDataAvailable(const base::string16& username,
                                       const base::string16& password) = 0;

  virtual void OnLoginModelDestroying() = 0;

 protected:
  virtual ~LoginModelObserver() {}
};

class LoginModel {
 public:
  // Add an observer interested in the data from the model.
  virtual void AddObserver(LoginModelObserver* observer) = 0;
  // Remove an observer from the model.
  virtual void RemoveObserver(LoginModelObserver* observer) = 0;

 protected:
  virtual ~LoginModel() {}
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_MODEL_H_
