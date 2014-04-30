// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class AutotestPrivateLogoutFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.logout", AUTOTESTPRIVATE_LOGOUT)

 private:
  virtual ~AutotestPrivateLogoutFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AutotestPrivateRestartFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.restart", AUTOTESTPRIVATE_RESTART)

 private:
  virtual ~AutotestPrivateRestartFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AutotestPrivateShutdownFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.shutdown",
                             AUTOTESTPRIVATE_SHUTDOWN)

 private:
  virtual ~AutotestPrivateShutdownFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AutotestPrivateLoginStatusFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.loginStatus",
                             AUTOTESTPRIVATE_LOGINSTATUS)

 private:
  virtual ~AutotestPrivateLoginStatusFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AutotestPrivateLockScreenFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.lockScreen",
                             AUTOTESTPRIVATE_LOCKSCREEN)

 private:
  virtual ~AutotestPrivateLockScreenFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AutotestPrivateGetExtensionsInfoFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getExtensionsInfo",
                             AUTOTESTPRIVATE_GETEXTENSIONSINFO)

 private:
  virtual ~AutotestPrivateGetExtensionsInfoFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AutotestPrivateSimulateAsanMemoryBugFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.simulateAsanMemoryBug",
                             AUTOTESTPRIVATE_SIMULATEASANMEMORYBUG)

 private:
  virtual ~AutotestPrivateSimulateAsanMemoryBugFunction() {}
  virtual bool RunSync() OVERRIDE;
};

// Don't kill the browser when we're in a browser test.
void SetAutotestPrivateTest();

// The profile-keyed service that manages the autotestPrivate extension API.
class AutotestPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
      GetFactoryInstance();

  // TODO(achuith): Replace these with a mock object for system calls.
  bool test_mode() const { return test_mode_; }
  void set_test_mode(bool test_mode) { test_mode_ = test_mode; }

 private:
  friend class BrowserContextKeyedAPIFactory<AutotestPrivateAPI>;

  AutotestPrivateAPI();
  virtual ~AutotestPrivateAPI();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "AutotestPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  bool test_mode_;  // true for ExtensionApiTest.AutotestPrivate browser test.
};

template <>
KeyedService*
    BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
        content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
