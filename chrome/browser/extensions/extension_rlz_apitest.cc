// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/win/registry.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_rlz_module.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "rlz/win/lib/rlz_lib.h"

class MockRlzSendFinancialPingFunction : public RlzSendFinancialPingFunction {
  virtual bool RunImpl();

  static int expected_count_;

 public:
  static int expected_count() {
    return expected_count_;
  }
};

int MockRlzSendFinancialPingFunction::expected_count_ = 0;

bool MockRlzSendFinancialPingFunction::RunImpl() {
  EXPECT_TRUE(RlzSendFinancialPingFunction::RunImpl());
  ++expected_count_;
  return true;
}

ExtensionFunction* MockRlzSendFinancialPingFunctionFactory() {
  return new MockRlzSendFinancialPingFunction();
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Rlz) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  // Before running the tests, clear the state of the RLZ products used.
  rlz_lib::AccessPoint access_points[] = {
    rlz_lib::GD_WEB_SERVER,
    rlz_lib::GD_OUTLOOK,
    rlz_lib::NO_ACCESS_POINT,
  };
  rlz_lib::ClearProductState(rlz_lib::PINYIN_IME, access_points);
  rlz_lib::ClearProductState(rlz_lib::DESKTOP, access_points);

  // Check that the state has really been cleared.
  base::win::RegKey key(HKEY_CURRENT_USER,
                        L"Software\\Google\\Common\\Rlz\\Events\\N",
                        KEY_READ);
  ASSERT_FALSE(key.Valid());

  key.Open(HKEY_CURRENT_USER, L"Software\\Google\\Common\\Rlz\\Events\\D",
           KEY_READ);
  ASSERT_FALSE(key.Valid());

  // Mock out experimental.rlz.sendFinancialPing().
  ASSERT_TRUE(ExtensionFunctionDispatcher::OverrideFunction(
      "experimental.rlz.sendFinancialPing",
      MockRlzSendFinancialPingFunctionFactory));

  // Set the access point that the test code is expecting.
  ASSERT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::GD_DESKBAND, "rlz_apitest"));

  // Now run all the tests.
  ASSERT_TRUE(RunExtensionTest("rlz")) << message_;

  ASSERT_EQ(3, MockRlzSendFinancialPingFunction::expected_count());
  ExtensionFunctionDispatcher::ResetFunctions();

  // Now make sure we recorded what was expected.  If the code in test.js
  // changes, need to make appropriate changes here.
  key.Open(HKEY_CURRENT_USER, L"Software\\Google\\Common\\Rlz\\Events\\N",
           KEY_READ);
  ASSERT_TRUE(key.Valid());

  DWORD value;
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"D3I", &value));
  ASSERT_EQ(1, value);
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"D3S", &value));
  ASSERT_EQ(1, value);
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"D3F", &value));
  ASSERT_EQ(1, value);

  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"D4I", &value));
  ASSERT_EQ(1, value);

  key.Open(HKEY_CURRENT_USER, L"Software\\Google\\Common\\Rlz\\Events\\D",
           KEY_READ);
  ASSERT_FALSE(key.Valid());

  // Cleanup.
  rlz_lib::ClearProductState(rlz_lib::PINYIN_IME, access_points);
}
