// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TEST_TEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TEST_TEST_API_H_

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"

template <typename T> struct DefaultSingletonTraits;

namespace extensions {

// A function that is only available in tests.
// Prior to running, checks that we are in an extension process.
class TestExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~TestExtensionFunction();

  // ExtensionFunction:
  virtual void Run() OVERRIDE;
};

class TestNotifyPassFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.notifyPass", UNKNOWN)

 protected:
  virtual ~TestNotifyPassFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class TestFailFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.notifyFail", UNKNOWN)

 protected:
  virtual ~TestFailFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class TestLogFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.log", UNKNOWN)

 protected:
  virtual ~TestLogFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class TestResetQuotaFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.resetQuota", UNKNOWN)

 protected:
  virtual ~TestResetQuotaFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class TestCreateIncognitoTabFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.createIncognitoTab", UNKNOWN)

 protected:
  virtual ~TestCreateIncognitoTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class TestSendMessageFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.sendMessage", UNKNOWN)

  // Sends a reply back to the calling extension. Many extensions don't need
  // a reply and will just ignore it.
  void Reply(const std::string& message);

 protected:
  virtual ~TestSendMessageFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class TestGetConfigFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.getConfig", UNKNOWN)

  // Set the dictionary returned by chrome.test.getConfig().
  // Does not take ownership of |value|.
  static void set_test_config_state(DictionaryValue* value);

 protected:
  // Tests that set configuration state do so by calling
  // set_test_config_state() as part of test set up, and unsetting it
  // during tear down.  This singleton class holds a pointer to that
  // state, owned by the test code.
  class TestConfigState {
   public:
    static TestConfigState* GetInstance();

    void set_config_state(DictionaryValue* config_state) {
      config_state_ = config_state;
    }

    const DictionaryValue* config_state() {
      return config_state_;
    }

   private:
    friend struct DefaultSingletonTraits<TestConfigState>;
    TestConfigState();

    DictionaryValue* config_state_;

    DISALLOW_COPY_AND_ASSIGN(TestConfigState);
  };

  virtual ~TestGetConfigFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TEST_TEST_API_H_
