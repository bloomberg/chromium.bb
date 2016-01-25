// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/switches.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"

using namespace chromeos::input_method;

namespace {

const char kLoginScreenUILanguage[] = "fr";
const char kInitialInputMethodOnLoginScreen[] = "xkb:us::eng";
const char kBackgroundReady[] = "ready";

// Class that listens for the JS message.
class TestListener : public content::NotificationObserver {
 public:
  TestListener() {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  ~TestListener() override {}

  // Implements the content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    const std::string& content = *content::Details<std::string>(details).ptr();
    if (content == kBackgroundReady) {
      // Initializes IMF for testing when receives ready message from
      // background.
      InputMethodManager* manager = InputMethodManager::Get();
      manager->GetInputMethodUtil()->InitXkbInputMethodsForTesting(
          *InputMethodWhitelist().GetSupportedInputMethods());

      std::vector<std::string> keyboard_layouts;
      keyboard_layouts.push_back(
          chromeos::extension_ime_util::GetInputMethodIDByEngineID(
              kInitialInputMethodOnLoginScreen));
      manager->GetActiveIMEState()->EnableLoginLayouts(kLoginScreenUILanguage,
                                                       keyboard_layouts);
    }
  }

 private:
  content::NotificationRegistrar registrar_;
};

class ExtensionInputMethodApiTest : public ExtensionApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ilanclmaeigfpnmdlgelmhkpkegdioip");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, Basic) {
  // Listener for extension's background ready.
  TestListener listener;

  ASSERT_TRUE(RunExtensionTest("input_method")) << message_;
}
