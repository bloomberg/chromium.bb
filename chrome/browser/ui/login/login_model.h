// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_MODEL_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_MODEL_H_
#pragma once

#include <string>

// Simple Model & Observer interfaces for a LoginView to facilitate exchanging
// information.
class LoginModelObserver {
 public:
  // Called by the model when a username,password pair has been identified
  // as a match for the pending login prompt.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) = 0;

 protected:
  virtual ~LoginModelObserver() {}
};

class LoginModel {
 public:
  // Set the observer interested in the data from the model.
  // observer can be null, signifying there is no longer any observer
  // interested in the data.
  virtual void SetObserver(LoginModelObserver* observer) = 0;

 protected:
  virtual ~LoginModel() {}
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_MODEL_H_
