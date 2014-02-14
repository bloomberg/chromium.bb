// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace base {
class TaskRunner;
}

namespace chromeos {

namespace {

// OOBE constants.
const char* kLocaleSelect = "language-select";
const char* kKeyboardSelect = "keyboard-select";

const char* kUSLayout = "xkb:us::eng";

}

namespace system {

// Custom StatisticsProvider that will return each set of region settings.
class FakeStatisticsProvider : public StatisticsProvider {
 public:
  virtual ~FakeStatisticsProvider() {}

  void set_locale(const std::string& locale) {
    initial_locale_ = locale;
  }

  void set_keyboard_layout(const std::string& keyboard_layout) {
    keyboard_layout_ = keyboard_layout;
  }

 private:
  // StatisticsProvider overrides.
  virtual void StartLoadingMachineStatistics(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      bool load_oem_manifest) OVERRIDE {
  }

  // Populates the named machine statistic for initial_locale and
  // keyboard_layout only.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) OVERRIDE {
    if (name == "initial_locale")
      *result = initial_locale_;
    else if (name == "keyboard_layout")
      *result = keyboard_layout_;
    else
      return false;

    return true;
  }

  virtual bool GetMachineFlag(const std::string& name, bool* result) OVERRIDE {
    return false;
  }

  virtual void Shutdown() OVERRIDE {
  }

  std::string initial_locale_;
  std::string keyboard_layout_;
};

}  // namespace system

class OobeLocalizationTest : public InProcessBrowserTest {
 public:
  OobeLocalizationTest();

  // Returns true if the expression was successfully executed and returned true.
  bool JsExecute(const std::string& expression);

  // Verifies that the comma-separated |values| corresponds with the first
  // values in |select_id|, optionally checking for an options group label after
  // the first set of options.
  void VerifyInitialOptions(const char* select_id,
                            const char* values,
                            bool check_separator);

  // Verifies that |value| exists in |select_id|.
  void VerifyOptionExists(const char* select_id, const char* value);

 protected:
  // Runs the test for the given locale and keyboard layout.
  void RunLocalizationTest(const std::string& initial_locale,
                           const std::string& keyboard_layout,
                           const std::string& expected_locale,
                           const std::string& expected_keyboard_layout);

 private:
  scoped_ptr<system::FakeStatisticsProvider> statistics_provider_;

  DISALLOW_COPY_AND_ASSIGN(OobeLocalizationTest);
};

OobeLocalizationTest::OobeLocalizationTest() {
  statistics_provider_.reset(new system::FakeStatisticsProvider());
  // Set the instance returned by GetInstance() for testing.
  system::StatisticsProvider::SetTestProvider(statistics_provider_.get());
}

bool OobeLocalizationTest::JsExecute(const std::string& expression) {
  chromeos::LoginDisplayHostImpl* display_host =
      static_cast<chromeos::LoginDisplayHostImpl*>(
          chromeos::LoginDisplayHostImpl::default_host());
  const std::string js("window.domAutomationController.send(!!(" +
                       expression + "));");

  bool result;
  if (content::ExecuteScriptAndExtractBool(
          display_host->GetOobeUI()->web_ui()->GetWebContents(), js, &result))
    return result;

  // Execution failed.
  return false;
}

void OobeLocalizationTest::VerifyInitialOptions(const char* select_id,
                                                const char* values,
                                                bool check_separator) {
  const std::string expression = base::StringPrintf(
      "(function () {\n"
      "  var select = document.querySelector('#%s');\n"
      "  if (!select)\n"
      "    return false;\n"
      "  var values = '%s'.split(',');\n"
      "  var correct = select.selectedIndex == 0;\n"
      "  for (var i = 0; i < values.length && correct; i++) {\n"
      "    if (select.options[i].value != values[i])\n"
      "      correct = false;\n"
      "  }\n"
      "  if (%d && correct)\n"
      "    correct = select.children[values.length].tagName === 'OPTGROUP';\n"
      "  return correct;\n"
      "})()", select_id, values, check_separator);
  ASSERT_TRUE(JsExecute(expression)) << expression;
}

void OobeLocalizationTest::VerifyOptionExists(const char* select_id,
                                              const char* value) {
  const std::string expression = base::StringPrintf(
      "(function () {\n"
      "  var select = document.querySelector('#%s');\n"
      "  if (!select)\n"
      "    return false;\n"
      "  for (var i = 0; i < select.options.length; i++) {\n"
      "    if (select.options[i].value == '%s')\n"
      "      return true;\n"
      "  }\n"
      "  return false;\n"
      "})()", select_id, value);
  ASSERT_TRUE(JsExecute(expression)) << expression;
}

void OobeLocalizationTest::RunLocalizationTest(
    const std::string& initial_locale,
    const std::string& keyboard_layout,
    const std::string& expected_locale,
    const std::string& expected_keyboard_layout) {
  statistics_provider_->set_locale(initial_locale);
  statistics_provider_->set_keyboard_layout(keyboard_layout);

  // Initialize StartupCustomizationDocument with fake statistics provider.
  StartupCustomizationDocument::GetInstance()->Init(
      statistics_provider_.get());

  // Bring up the OOBE network screen.
  chromeos::ShowLoginWizard(chromeos::WizardController::kNetworkScreenName);
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();

  VerifyInitialOptions(kLocaleSelect, expected_locale.c_str(), true);
  VerifyInitialOptions(kKeyboardSelect,
                       expected_keyboard_layout.c_str(),
                       false);

  // Make sure we have a fallback keyboard.
  VerifyOptionExists(kKeyboardSelect, kUSLayout);

  // Shut down the display host.
  chromeos::LoginDisplayHostImpl::default_host()->Finalize();
  base::MessageLoopForUI::current()->RunUntilIdle();

  // Clear the locale pref so the statistics provider is pinged next time.
  g_browser_process->local_state()->SetString(prefs::kApplicationLocale,
                                              std::string());
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenNonLatin) {
  // For a non-Latin keyboard layout like Russian, we expect to see the US
  // keyboard.
  RunLocalizationTest("ru", "xkb:ru::rus",
                      "ru", kUSLayout);

  // IMEs do not load at OOBE, so we just expect to see the (Latin) Japanese
  // keyboard.
  RunLocalizationTest("ja", "xkb:jp::jpn",
                      "ja", "xkb:jp::jpn");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenKeyboardLayout) {
  // We don't use the Icelandic locale but the Icelandic keyboard layout
  // should still be selected when specified as the default.
  RunLocalizationTest("en-US", "xkb:is::ice",
                      "en-US", "xkb:is::ice");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenFullLatin) {
  // French Swiss keyboard.
  RunLocalizationTest("fr", "xkb:ch:fr:fra",
                      "fr", "xkb:ch:fr:fra");

  // German Swiss keyboard.
  RunLocalizationTest("de", "xkb:ch::ger",
                      "de", "xkb:ch::ger");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenMultipleLocales) {
 RunLocalizationTest("es,en-US,nl", "xkb:be::nld",
                     "es,en-US,nl", "xkb:be::nld");

 RunLocalizationTest("ru,de", "xkb:ru::rus",
                     "ru,de", kUSLayout);
}

}  // namespace chromeos
