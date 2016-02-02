// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace extensions {

static const char kHotwordHelperExtensionId[] =
    "dnhpdliibojhegemfjheidglijccjfmc";

class HotwordBrowserTest : public ExtensionBrowserTest {
 public:
  HotwordBrowserTest() : error_console_(NULL) { }
  ~HotwordBrowserTest() override {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    // Load the hotword_helper extension.
    ComponentLoader::EnableBackgroundExtensionsForTesting();

    // We need to enable the ErrorConsole FeatureSwitch in order to collect
    // errors. This should be enabled on any channel <= Dev, but let's make
    // sure (in case a test is running on, e.g., a beta channel).
    FeatureSwitch::error_console()->SetOverrideValue(
        FeatureSwitch::OVERRIDE_ENABLED);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Errors are only kept if we have Developer Mode enabled.
    profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

    error_console_ = ErrorConsole::Get(profile());
    ASSERT_TRUE(error_console_);
  }

  ErrorConsole* error_console() { return error_console_; }

 private:
  // Weak reference to the ErrorConsole.
  ErrorConsole* error_console_;

  DISALLOW_COPY_AND_ASSIGN(HotwordBrowserTest);
};

// Test we silently capture an exception from a message handler's response
// callback. This happens when the caller to chrome.runtime.sendMessage()
// doesn't specify a response callback.
// NOTE(amistry): Test is disabled instead of deleted since the functionality
// may still be required to implement crbug.com/436681
IN_PROC_BROWSER_TEST_F(HotwordBrowserTest, DISABLED_MessageSendResponseError) {
  // Enable error reporting for the hotword helper extension.
  error_console()->SetReportingAllForExtension(kHotwordHelperExtensionId, true);

  ExtensionTestMessageListener doneListener("done", false);
  const Extension* extension = extension_service()->GetExtensionById(
      kHotwordHelperExtensionId, false);
  ASSERT_TRUE(extension);
  const Extension* test_extension = LoadExtension(
      test_data_dir_.AppendASCII("hotword"));
  ASSERT_TRUE(test_extension);

  ASSERT_TRUE(doneListener.WaitUntilSatisfied());
  ASSERT_TRUE(error_console()->GetErrorsForExtension(extension->id()).empty());
}

}  // namespace extensions
