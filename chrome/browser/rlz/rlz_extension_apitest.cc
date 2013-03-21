// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/rlz/rlz_extension_api.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "rlz/lib/rlz_lib.h"

#if (OS_WIN)
#include "base/win/registry.h"
#endif

class MockRlzSendFinancialPingFunction : public RlzSendFinancialPingFunction {
 public:
  static int expected_count() {
    return expected_count_;
  }

 protected:
  virtual ~MockRlzSendFinancialPingFunction() {}

  // ExtensionFunction
  virtual bool RunImpl() OVERRIDE;

 private:
  static int expected_count_;
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

// Mac is flaky - http://crbug.com/137834. ChromeOS is not supported yet.
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define MAYBE_Rlz DISABLED_Rlz
#else
#define MAYBE_Rlz Rlz
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Rlz) {
  // The default test resolver doesn't allow lookups to *.google.com. That
  // makes sense, but it does make RLZ's SendFinancialPing() fail -- so allow
  // connections to google.com in this test.
  scoped_refptr<net::RuleBasedHostResolverProc> resolver =
      new net::RuleBasedHostResolverProc(host_resolver());
  resolver->AllowDirectLookup("*.google.com");
  net::ScopedDefaultHostResolverProc scoper(resolver);

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

#if defined(OS_WIN)
  // Check that the state has really been cleared.
  base::win::RegKey key(HKEY_CURRENT_USER,
                        L"Software\\Google\\Common\\Rlz\\Events\\N",
                        KEY_READ);
  ASSERT_FALSE(key.Valid());

  key.Open(HKEY_CURRENT_USER, L"Software\\Google\\Common\\Rlz\\Events\\D",
           KEY_READ);
  ASSERT_FALSE(key.Valid());
#endif

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

#if defined(OS_WIN)
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
#endif

  // Cleanup.
  rlz_lib::ClearProductState(rlz_lib::PINYIN_IME, access_points);
}
