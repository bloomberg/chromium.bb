// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_LOGGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_LOGGER_H_

#include <string>
#include "base/macros.h"

// This interface is used by the password management code to report on progress
// of actions like saving a password.
class PasswordManagerLogger {
 public:
  PasswordManagerLogger() {}
  virtual ~PasswordManagerLogger() {}

  virtual void LogSavePasswordProgress(const std::string& text) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerLogger);
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_LOGGER_H_
