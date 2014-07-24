// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_TEST_API_H_
#define EXTENSIONS_BROWSER_API_TEST_TEST_API_H_

#include "base/values.h"
#include "extensions/browser/extension_function.h"

template <typename T>
struct DefaultSingletonTraits;

namespace extensions {

// A function that is only available in tests.
// Prior to running, checks that we are in a testing process.
class TestExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~TestExtensionFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() OVERRIDE;

  virtual bool RunSafe() = 0;
};

class TestNotifyPassFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.notifyPass", UNKNOWN)

 protected:
  virtual ~TestNotifyPassFunction();

  // TestExtensionFunction:
  virtual bool RunSafe() OVERRIDE;
};

class TestNotifyFailFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.notifyFail", UNKNOWN)

 protected:
  virtual ~TestNotifyFailFunction();

  // TestExtensionFunction:
  virtual bool RunSafe() OVERRIDE;
};

class TestLogFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.log", UNKNOWN)

 protected:
  virtual ~TestLogFunction();

  // TestExtensionFunction:
  virtual bool RunSafe() OVERRIDE;
};

class TestSendMessageFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.sendMessage", UNKNOWN)

  // Sends a reply back to the calling extension. Many extensions don't need
  // a reply and will just ignore it.
  void Reply(const std::string& message);

  // Sends an error back to the calling extension.
  void ReplyWithError(const std::string& error);

 protected:
  virtual ~TestSendMessageFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class TestGetConfigFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.getConfig", UNKNOWN)

  // Set the dictionary returned by chrome.test.getConfig().
  // Does not take ownership of |value|.
  static void set_test_config_state(base::DictionaryValue* value);

 protected:
  // Tests that set configuration state do so by calling
  // set_test_config_state() as part of test set up, and unsetting it
  // during tear down.  This singleton class holds a pointer to that
  // state, owned by the test code.
  class TestConfigState {
   public:
    static TestConfigState* GetInstance();

    void set_config_state(base::DictionaryValue* config_state) {
      config_state_ = config_state;
    }

    const base::DictionaryValue* config_state() { return config_state_; }

   private:
    friend struct DefaultSingletonTraits<TestConfigState>;
    TestConfigState();

    base::DictionaryValue* config_state_;

    DISALLOW_COPY_AND_ASSIGN(TestConfigState);
  };

  virtual ~TestGetConfigFunction();

  // TestExtensionFunction:
  virtual bool RunSafe() OVERRIDE;
};

class TestWaitForRoundTripFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.waitForRoundTrip", UNKNOWN)

 protected:
  virtual ~TestWaitForRoundTripFunction();

  // TestExtensionFunction:
  virtual bool RunSafe() OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_TEST_API_H_
