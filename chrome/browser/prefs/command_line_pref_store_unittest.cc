// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace {

class TestCommandLinePrefStore : public CommandLinePrefStore {
 public:
  explicit TestCommandLinePrefStore(CommandLine* cl)
      : CommandLinePrefStore(cl) {}

  bool ProxySwitchesAreValid() {
    return ValidateProxySwitches();
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
  CommandLinePrefStore store(&cl);

  Value* actual = NULL;
  ASSERT_EQ(PrefStore::READ_OK, store.GetValue(prefs::kNoProxyServer, &actual));
  bool result;
  EXPECT_TRUE(actual->GetAsBoolean(&result));
  EXPECT_TRUE(result);
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
  CommandLinePrefStore store(&cl);

  Value* actual = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE, store.GetValue(unknown_bool, &actual));
  ASSERT_EQ(PrefStore::READ_OK,
            store.GetValue(prefs::kProxyAutoDetect, &actual));
  bool bool_result = false;
  EXPECT_TRUE(actual->GetAsBoolean(&bool_result));
  EXPECT_TRUE(bool_result);

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
