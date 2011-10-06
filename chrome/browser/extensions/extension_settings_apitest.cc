// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

class ExtensionSettingsApiTest : public ExtensionApiTest {
 protected:
  void ReplyWhenSatisfied(
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(
        normal_action, incognito_action, NULL, false);
  }

  void LoadAndReplyWhenSatisfied(
      const std::string& normal_action,
      const std::string& incognito_action,
      const std::string& extension_dir) {
    MaybeLoadAndReplyWhenSatisfied(
        normal_action, incognito_action, &extension_dir, false);
  }

  void FinalReplyWhenSatisfied(
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(normal_action, incognito_action, NULL, true);
  }

 private:
  void MaybeLoadAndReplyWhenSatisfied(
      const std::string& normal_action,
      const std::string& incognito_action,
      // May be NULL to imply not loading the extension.
      const std::string* extension_dir,
      bool is_final_action) {
    ExtensionTestMessageListener listener("waiting", true);
    ExtensionTestMessageListener listener_incognito("waiting_incognito", true);

    // Only load the extension after the listeners have been set up, to avoid
    // initialisation race conditions.
    if (extension_dir) {
      ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
          .AppendASCII("settings").AppendASCII(*extension_dir)));
    }

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

    listener.Reply(CreateMessage(normal_action, is_final_action));
    listener_incognito.Reply(CreateMessage(incognito_action, is_final_action));
  }

  std::string CreateMessage(const std::string& action, bool is_final_action) {
    scoped_ptr<DictionaryValue> message(new DictionaryValue());
    message->SetString("action", action);
    message->SetBoolean("isFinalAction", is_final_action);
    std::string message_json;
    base::JSONWriter::Write(message.get(), false, &message_json);
    return message_json;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SimpleTest) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  ASSERT_TRUE(RunExtensionTest("settings/simple_test")) << message_;
}

// Structure of this test taken from DISABLED_IncognitoSplitMode, so it's
// likely that they will need to be disabled on the bots, too.  See
// http://crbug.com/53991.
//
// Note that only split-mode incognito is tested, because spanning mode
// incognito looks the same as normal mode when the only API activity comes
// from background pages.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SplitModeIncognito) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied("assertEmpty", "assertEmpty", "split_incognito");
  ReplyWhenSatisfied("noop", "setFoobar");
  ReplyWhenSatisfied("assertFoobar", "assertFoobar");
  ReplyWhenSatisfied("clear", "noop");
  ReplyWhenSatisfied("assertEmpty", "assertEmpty");
  ReplyWhenSatisfied("setFoobar", "noop");
  ReplyWhenSatisfied("assertFoobar", "assertFoobar");
  ReplyWhenSatisfied("noop", "removeFoo");
  FinalReplyWhenSatisfied("assertEmpty", "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}
