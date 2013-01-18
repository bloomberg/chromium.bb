// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

namespace extensions {

class AutotestPrivateLogoutFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.logout", AUTOTESTPRIVATE_LOGOUT)

 private:
  virtual ~AutotestPrivateLogoutFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AutotestPrivateRestartFunction: public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.restart", AUTOTESTPRIVATE_RESTART)

 private:
  virtual ~AutotestPrivateRestartFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AutotestPrivateShutdownFunction: public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.shutdown",
                             AUTOTESTPRIVATE_SHUTDOWN)

 private:
  virtual ~AutotestPrivateShutdownFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AutotestPrivateLoginStatusFunction: public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.loginStatus",
                             AUTOTESTPRIVATE_LOGINSTATUS)

 private:
  virtual ~AutotestPrivateLoginStatusFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

// Don't kill the browser when we're in a browser test.
void SetAutotestPrivateTest();

// The profile-keyed service that manages the autotestPrivate extension API.
class AutotestPrivateAPI : public ProfileKeyedService {
 public:
  AutotestPrivateAPI();

  // TODO(achuith): Replace these with a mock object for system calls.
  bool test_mode() const { return test_mode_; }
  void set_test_mode(bool test_mode) { test_mode_ = test_mode; }

 private:
  virtual ~AutotestPrivateAPI();

  bool test_mode_;  // true for ExtensionApiTest.AutotestPrivate browser test.
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
