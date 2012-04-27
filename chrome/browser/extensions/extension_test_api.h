// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"

template <typename T> struct DefaultSingletonTraits;

// A function that is only available in tests.
// Prior to running, checks that we are in an extension process.
class TestExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~TestExtensionFunction();

  // ExtensionFunction:
  virtual void Run() OVERRIDE;
};

class ExtensionTestPassFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.notifyPass")

 protected:
  virtual ~ExtensionTestPassFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ExtensionTestFailFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.notifyFail")

 protected:
  virtual ~ExtensionTestFailFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ExtensionTestLogFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.log")

 protected:
  virtual ~ExtensionTestLogFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ExtensionTestQuotaResetFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.resetQuota")

 protected:
  virtual ~ExtensionTestQuotaResetFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ExtensionTestCreateIncognitoTabFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.createIncognitoTab")

 protected:
  virtual ~ExtensionTestCreateIncognitoTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ExtensionTestSendMessageFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.sendMessage")

  // Sends a reply back to the calling extension. Many extensions don't need
  // a reply and will just ignore it.
  void Reply(const std::string& message);

 protected:
  virtual ~ExtensionTestSendMessageFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ExtensionTestGetConfigFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("test.getConfig")

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

  virtual ~ExtensionTestGetConfigFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
