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
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kApplicationLocale, &result));
  EXPECT_EQ("hi-MOM", result);
}

// Tests a simple boolean pref on the command line.
TEST(CommandLinePrefStoreTest, SimpleBooleanPref) {
  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitch(switches::kNoProxyServer);
  CommandLinePrefStore store(&cl);
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  bool result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &result));
  EXPECT_TRUE(result);
}

// Tests a command line with no recognized prefs.
TEST(CommandLinePrefStoreTest, NoPrefs) {
  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitch(unknown_string);
  cl.AppendSwitchASCII(unknown_bool, "a value");
  CommandLinePrefStore store(&cl);
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  bool bool_result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(unknown_bool, &bool_result));
  EXPECT_FALSE(bool_result);

  std::string string_result = "";
  EXPECT_FALSE(store.prefs()->GetString(unknown_string, &string_result));
  EXPECT_EQ("", string_result);
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
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  bool bool_result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(unknown_bool, &bool_result));
  EXPECT_FALSE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect, &bool_result));
  EXPECT_TRUE(bool_result);

  std::string string_result = "";
  EXPECT_FALSE(store.prefs()->GetString(unknown_string, &string_result));
  EXPECT_EQ("", string_result);
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  EXPECT_EQ("proxy", string_result);
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
      &string_result));
  EXPECT_EQ("list", string_result);
}

// Tests proxy switch validation.
TEST(CommandLinePrefStoreTest, ProxySwitchValidation) {
  CommandLine cl(CommandLine::NO_PROGRAM);

  // No switches.
  TestCommandLinePrefStore store(&cl);
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  EXPECT_TRUE(store.ProxySwitchesAreValid());

  // Only no-proxy.
  cl.AppendSwitch(switches::kNoProxyServer);
  TestCommandLinePrefStore store2(&cl);
  EXPECT_EQ(store2.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  EXPECT_TRUE(store2.ProxySwitchesAreValid());

  // Another proxy switch too.
  cl.AppendSwitch(switches::kProxyAutoDetect);
  TestCommandLinePrefStore store3(&cl);
  EXPECT_EQ(store3.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  EXPECT_FALSE(store3.ProxySwitchesAreValid());

  // All proxy switches except no-proxy.
  CommandLine cl2(CommandLine::NO_PROGRAM);
  cl2.AppendSwitch(switches::kProxyAutoDetect);
  cl2.AppendSwitchASCII(switches::kProxyServer, "server");
  cl2.AppendSwitchASCII(switches::kProxyPacUrl, "url");
  cl2.AppendSwitchASCII(switches::kProxyBypassList, "list");
  TestCommandLinePrefStore store4(&cl2);
  EXPECT_EQ(store4.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  EXPECT_TRUE(store4.ProxySwitchesAreValid());
}
