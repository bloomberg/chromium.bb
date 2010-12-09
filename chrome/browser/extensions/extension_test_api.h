// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"

template <typename T> struct DefaultSingletonTraits;

class ExtensionTestPassFunction : public SyncExtensionFunction {
  ~ExtensionTestPassFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.notifyPass")
};

class ExtensionTestFailFunction : public SyncExtensionFunction {
  ~ExtensionTestFailFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.notifyFail")
};

class ExtensionTestLogFunction : public SyncExtensionFunction {
  ~ExtensionTestLogFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.log")
};

class ExtensionTestQuotaResetFunction : public SyncExtensionFunction {
  ~ExtensionTestQuotaResetFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.resetQuota")
};

class ExtensionTestCreateIncognitoTabFunction : public SyncExtensionFunction {
  ~ExtensionTestCreateIncognitoTabFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.createIncognitoTab")
};

class ExtensionTestSendMessageFunction : public AsyncExtensionFunction {
 public:
  // Sends a reply back to the calling extension. Many extensions don't need
  // a reply and will just ignore it.
  void Reply(const std::string& message);

 private:
  ~ExtensionTestSendMessageFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.sendMessage")
};

class ExtensionTestGetConfigFunction : public SyncExtensionFunction {
 public:
  // Set the dictionary returned by chrome.test.getConfig().
  // Does not take ownership of |value|.
  static void set_test_config_state(DictionaryValue* value);

 private:
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

  ~ExtensionTestGetConfigFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.getConfig")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
