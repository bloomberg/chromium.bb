// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/base/ui_base_switches.h"

namespace {

class TestCommandLinePrefStore : public CommandLinePrefStore {
 public:
  explicit TestCommandLinePrefStore(CommandLine* cl)
      : CommandLinePrefStore(cl) {}

  bool ProxySwitchesAreValid() {
    return ValidateProxySwitches();
  }

  void VerifyIntPref(const std::string& path, int expected_value) {
    Value* actual = NULL;
    ASSERT_EQ(PrefStore::READ_OK, GetValue(path, &actual));
    EXPECT_TRUE(FundamentalValue(expected_value).Equals(actual));
  }
};

const char unknown_bool[] = "unknown_switch";
const char unknown_string[] = "unknown_other_switch";

}  // namespace

// Tests a simple string pref on the command line.
TEST(CommandLinePrefStoreTest, SimpleStringPref) {
  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(switches::kLang, "hi-MOM");
  CommandLinePrefStore store(&cl);

  Value* actual = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store.GetValue(prefs::kApplicationLocale, &actual));
  std::string result;
  EXPECT_TRUE(actual->GetAsString(&result));
  EXPECT_EQ("hi-MOM", result);
}

// Tests a simple boolean pref on the command line.
TEST(CommandLinePrefStoreTest, SimpleBooleanPref) {
  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitch(switches::kNoProxyServer);
  TestCommandLinePrefStore store(&cl);

  store.VerifyIntPref(prefs::kProxyMode, ProxyPrefs::MODE_DIRECT);
}

// Tests a command line with no recognized prefs.
TEST(CommandLinePrefStoreTest, NoPrefs) {
  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitch(unknown_string);
  cl.AppendSwitchASCII(unknown_bool, "a value");
  CommandLinePrefStore store(&cl);

  Value* actual = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE, store.GetValue(unknown_bool, &actual));
  EXPECT_EQ(PrefStore::READ_NO_VALUE, store.GetValue(unknown_string, &actual));
}

// Tests a complex command line with multiple known and unknown switches.
TEST(CommandLinePrefStoreTest, MultipleSwitches) {
  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitch(unknown_string);
  cl.AppendSwitch(switches::kProxyAutoDetect);
  cl.AppendSwitchASCII(switches::kProxyServer, "proxy");
  cl.AppendSwitchASCII(switches::kProxyBypassList, "list");
  cl.AppendSwitchASCII(unknown_bool, "a value");
  TestCommandLinePrefStore store(&cl);

  Value* actual = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE, store.GetValue(unknown_bool, &actual));
  store.VerifyIntPref(prefs::kProxyMode, ProxyPrefs::MODE_AUTO_DETECT);

  EXPECT_EQ(PrefStore::READ_NO_VALUE, store.GetValue(unknown_string, &actual));
  std::string string_result = "";
  ASSERT_EQ(PrefStore::READ_OK, store.GetValue(prefs::kProxyServer, &actual));
  EXPECT_TRUE(actual->GetAsString(&string_result));
  EXPECT_EQ("proxy", string_result);
  ASSERT_EQ(PrefStore::READ_OK,
            store.GetValue(prefs::kProxyBypassList, &actual));
  EXPECT_TRUE(actual->GetAsString(&string_result));
  EXPECT_EQ("list", string_result);
}

// Tests proxy switch validation.
TEST(CommandLinePrefStoreTest, ProxySwitchValidation) {
  CommandLine cl(CommandLine::NO_PROGRAM);

  // No switches.
  TestCommandLinePrefStore store(&cl);
  EXPECT_TRUE(store.ProxySwitchesAreValid());

  // Only no-proxy.
  cl.AppendSwitch(switches::kNoProxyServer);
  TestCommandLinePrefStore store2(&cl);
  EXPECT_TRUE(store2.ProxySwitchesAreValid());

  // Another proxy switch too.
  cl.AppendSwitch(switches::kProxyAutoDetect);
  TestCommandLinePrefStore store3(&cl);
  EXPECT_FALSE(store3.ProxySwitchesAreValid());

  // All proxy switches except no-proxy.
  CommandLine cl2(CommandLine::NO_PROGRAM);
  cl2.AppendSwitch(switches::kProxyAutoDetect);
  cl2.AppendSwitchASCII(switches::kProxyServer, "server");
  cl2.AppendSwitchASCII(switches::kProxyPacUrl, "url");
  cl2.AppendSwitchASCII(switches::kProxyBypassList, "list");
  TestCommandLinePrefStore store4(&cl2);
  EXPECT_TRUE(store4.ProxySwitchesAreValid());
}

TEST(CommandLinePrefStoreTest, ManualProxyModeInference) {
  CommandLine cl1(CommandLine::NO_PROGRAM);
  cl1.AppendSwitch(unknown_string);
  cl1.AppendSwitchASCII(switches::kProxyServer, "proxy");
  TestCommandLinePrefStore store1(&cl1);
  store1.VerifyIntPref(prefs::kProxyMode, ProxyPrefs::MODE_FIXED_SERVERS);

  CommandLine cl2(CommandLine::NO_PROGRAM);
  cl2.AppendSwitchASCII(switches::kProxyPacUrl, "proxy");
  TestCommandLinePrefStore store2(&cl2);
  store2.VerifyIntPref(prefs::kProxyMode, ProxyPrefs::MODE_PAC_SCRIPT);
}
