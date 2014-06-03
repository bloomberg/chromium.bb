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
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/input_method_whitelist.h"
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
  virtual ~OobeLocalizationTest();

  // Verifies that the comma-separated |values| corresponds with the first
  // values in |select_id|, optionally checking for an options group label after
  // the first set of options.
  bool VerifyInitialOptions(const char* select_id,
                            const char* values,
                            bool check_separator);

  // Verifies that |value| exists in |select_id|.
  bool VerifyOptionExists(const char* select_id, const char* value);

  // Dumps OOBE select control (language or keyboard) to string.
  std::string DumpOptions(const char* select_id);

 protected:
  // Runs the test for the given locale and keyboard layout.
  void RunLocalizationTest(const std::string& initial_locale,
                           const std::string& keyboard_layout,
                           const std::string& expected_locale,
                           const std::string& expected_keyboard_layout,
                           const std::string& expected_keyboard_select_control);

 private:
  scoped_ptr<system::FakeStatisticsProvider> statistics_provider_;
  test::JSChecker checker;

  DISALLOW_COPY_AND_ASSIGN(OobeLocalizationTest);
};

OobeLocalizationTest::OobeLocalizationTest() {
  statistics_provider_.reset(new system::FakeStatisticsProvider());
  // Set the instance returned by GetInstance() for testing.
  system::StatisticsProvider::SetTestProvider(statistics_provider_.get());
}

OobeLocalizationTest::~OobeLocalizationTest() {
  system::StatisticsProvider::SetTestProvider(NULL);
}

bool OobeLocalizationTest::VerifyInitialOptions(const char* select_id,
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
  const bool execute_status = checker.GetBool(expression);
  EXPECT_TRUE(execute_status) << expression;
  return execute_status;
}

bool OobeLocalizationTest::VerifyOptionExists(const char* select_id,
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
  const bool execute_status = checker.GetBool(expression);
  EXPECT_TRUE(execute_status) << expression;
  return execute_status;
}

std::string OobeLocalizationTest::DumpOptions(const char* select_id) {
  const std::string expression = base::StringPrintf(
      "\n"
      "(function () {\n"
      "  var selector = '#%s';\n"
      "  var divider = ',';\n"
      "  var select = document.querySelector(selector);\n"
      "  if (!select)\n"
      "    return 'document.querySelector(' + selector + ') failed.';\n"
      "  var dumpOptgroup = function(group) {\n"
      "    var result = '';\n"
      "    for (var i = 0; i < group.children.length; i++) {\n"
      "      if (i > 0) {\n"
      "        result += divider;\n"
      "      }\n"
      "      if (group.children[i].value) {\n"
      "        result += group.children[i].value;\n"
      "      } else {\n"
      "        result += '__NO_VALUE__';\n"
      "      }\n"
      "    }\n"
      "    return result;\n"
      "  };\n"
      "  var result = '';\n"
      "  if (select.selectedIndex != 0) {\n"
      "    result += '(selectedIndex=' + select.selectedIndex + \n"
      "        ', selected \"' + select.options[select.selectedIndex].value +\n"
      "        '\")';\n"
      "  }\n"
      "  var children = select.children;\n"
      "  for (var i = 0; i < children.length; i++) {\n"
      "    if (i > 0) {\n"
      "      result += divider;\n"
      "    }\n"
      "    if (children[i].value) {\n"
      "      result += children[i].value;\n"
      "    } else if (children[i].tagName === 'OPTGROUP') {\n"
      "      result += '[' + dumpOptgroup(children[i]) + ']';\n"
      "    } else {\n"
      "      result += '__NO_VALUE__';\n"
      "    }\n"
      "  }\n"
      "  return result;\n"
      "})()\n",
      select_id);
  return checker.GetString(expression);
}

std::string TranslateXKB2Extension(const std::string& src) {
  std::string result(src);
  // Modifies the expected keyboard select control options for the new
  // extension based xkb id.
  size_t pos = 0;
  std::string repl_old = "xkb:";
  std::string repl_new =
      extension_ime_util::GetInputMethodIDByEngineID("xkb:");
  while ((pos = result.find(repl_old, pos)) != std::string::npos) {
    result.replace(pos, repl_old.length(), repl_new);
    pos += repl_new.length();
  }
  return result;
}

void OobeLocalizationTest::RunLocalizationTest(
    const std::string& initial_locale,
    const std::string& keyboard_layout,
    const std::string& expected_locale,
    const std::string& expected_keyboard_layout,
    const std::string& expected_keyboard_select_control) {
  statistics_provider_->set_locale(initial_locale);
  statistics_provider_->set_keyboard_layout(keyboard_layout);

  // Initialize StartupCustomizationDocument with fake statistics provider.
  StartupCustomizationDocument::GetInstance()->Init(
      statistics_provider_.get());

  g_browser_process->local_state()->SetString(
      prefs::kHardwareKeyboardLayout, keyboard_layout);

  input_method::InputMethodManager::Get()
      ->GetInputMethodUtil()
      ->InitXkbInputMethodsForTesting();

  const std::string expected_keyboard_select =
      TranslateXKB2Extension(expected_keyboard_select_control);

  // Bring up the OOBE network screen.
  chromeos::ShowLoginWizard(chromeos::WizardController::kNetworkScreenName);
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();

  checker.set_web_contents(static_cast<chromeos::LoginDisplayHostImpl*>(
                           chromeos::LoginDisplayHostImpl::default_host())->
                           GetOobeUI()->web_ui()->GetWebContents());

  if (!VerifyInitialOptions(kLocaleSelect, expected_locale.c_str(), true)) {
    LOG(ERROR) << "Actual value of " << kLocaleSelect << ":\n"
               << DumpOptions(kLocaleSelect);
  }
  if (!VerifyInitialOptions(
          kKeyboardSelect,
          TranslateXKB2Extension(expected_keyboard_layout).c_str(),
          false)) {
    LOG(ERROR) << "Actual value of " << kKeyboardSelect << ":\n"
               << DumpOptions(kKeyboardSelect);
  }

  // Make sure we have a fallback keyboard.
  if (!VerifyOptionExists(kKeyboardSelect,
                          extension_ime_util::GetInputMethodIDByEngineID(
                              kUSLayout).c_str())) {
    LOG(ERROR) << "Actual value of " << kKeyboardSelect << ":\n"
               << DumpOptions(kKeyboardSelect);
  }

  // Note, that sort order is locale-specific, but is unlikely to change.
  // Especially for keyboard layouts.
  EXPECT_EQ(expected_keyboard_select, DumpOptions(kKeyboardSelect));

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
                      "ru", kUSLayout,
                      "xkb:us::eng");

  RunLocalizationTest("ru", "xkb:us::eng,xkb:ru::rus",
                      "ru", kUSLayout,
                      "xkb:us::eng");

  // IMEs do not load at OOBE, so we just expect to see the (Latin) Japanese
  // keyboard.
  RunLocalizationTest("ja", "xkb:jp::jpn",
                      "ja", "xkb:jp::jpn",
                      "xkb:jp::jpn,[xkb:us::eng]");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenKeyboardLayout) {
  // We don't use the Icelandic locale but the Icelandic keyboard layout
  // should still be selected when specified as the default.
  RunLocalizationTest("en-US", "xkb:is::ice",
                      "en-US", "xkb:is::ice",
                      "xkb:is::ice,["
                          "xkb:us::eng,xkb:us:intl:eng,xkb:us:altgr-intl:eng,"
                          "xkb:us:dvorak:eng,xkb:us:colemak:eng]");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenFullLatin) {
  // French Swiss keyboard.
  RunLocalizationTest("fr", "xkb:ch:fr:fra",
                      "fr", "xkb:ch:fr:fra",
                      "xkb:ch:fr:fra,["
                          "xkb:fr::fra,xkb:be::fra,xkb:ca::fra,"
                          "xkb:ca:multix:fra,xkb:us::eng]");

  // German Swiss keyboard.
  RunLocalizationTest("de", "xkb:ch::ger",
                      "de", "xkb:ch::ger",
                      "xkb:ch::ger,["
                          "xkb:de::ger,xkb:de:neo:ger,xkb:be::ger,xkb:us::eng"
                      "]");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenMultipleLocales) {
  RunLocalizationTest("es,en-US,nl", "xkb:be::nld",
                      "es,en-US,nl", "xkb:be::nld",
                      "xkb:be::nld,[xkb:es::spa,xkb:latam::spa,xkb:us::eng]");

  RunLocalizationTest("ru,de", "xkb:ru::rus",
                      "ru,de", kUSLayout,
                      "xkb:us::eng");
}

IN_PROC_BROWSER_TEST_F(OobeLocalizationTest, NetworkScreenRegionalLocales) {
  // Syntetic example to test correct merging of different locales.
  RunLocalizationTest("fr-CH,it-CH,de-CH",
                      "xkb:fr::fra,xkb:it::ita,xkb:de::ger",
                      "fr-CH,it-CH,de-CH",
                      "xkb:fr::fra",
                      "xkb:fr::fra,xkb:it::ita,xkb:de::ger,["
                          "xkb:be::fra,xkb:ca::fra,xkb:ch:fr:fra,"
                          "xkb:ca:multix:fra,xkb:us::eng"
                      "]");
  // Another syntetic example. Check that british keyboard is available.
  RunLocalizationTest("en-AU",
                      "xkb:us::eng",
                      "en-AU",
                      "xkb:us::eng",
                      "xkb:us::eng,[xkb:gb:extd:eng,xkb:gb:dvorak:eng]");
}

}  // namespace chromeos
