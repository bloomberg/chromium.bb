// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef InProcessBrowserTest FirstRunBrowserTest;

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowFirstRunBubblePref) {
  EXPECT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShowFirstRunBubbleOption));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_DONT_SHOW,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_SHOW));
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShowFirstRunBubbleOption));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_SHOW,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  // Test that toggling the value works in either direction after it's been set.
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_DONT_SHOW));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_DONT_SHOW,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  // Test that the value can't be set to FIRST_RUN_BUBBLE_SHOW after it has been
  // set to FIRST_RUN_BUBBLE_SUPPRESS.
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_SUPPRESS));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_SUPPRESS,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_SHOW));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_SUPPRESS,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
}

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShouldShowWelcomePage) {
  EXPECT_FALSE(first_run::ShouldShowWelcomePage());
  first_run::SetShouldShowWelcomePage();
  EXPECT_TRUE(first_run::ShouldShowWelcomePage());
  EXPECT_FALSE(first_run::ShouldShowWelcomePage());
}

#if !defined(OS_CHROMEOS)
namespace {

// A generic test class to be subclassed by test classes testing specific
// master_preferences. All subclasses must call SetMasterPreferencesForTest()
// from their SetUp() method before deferring the remainder of Setup() to this
// class.
class FirstRunMasterPrefsBrowserTestBase : public InProcessBrowserTest {
 public:
  FirstRunMasterPrefsBrowserTestBase() {}

 protected:
  virtual void SetUp() OVERRIDE {
    // All users of this test class need to call SetMasterPreferencesForTest()
    // before this class' SetUp() is invoked.
    ASSERT_TRUE(text_.get());

    ASSERT_TRUE(file_util::CreateTemporaryFile(&prefs_file_));
    EXPECT_TRUE(file_util::WriteFile(prefs_file_, text_->c_str(),
                                     text_->size()));
    first_run::SetMasterPrefsPathForTesting(prefs_file_);

    // This invokes BrowserMain, and does the import, so must be done last.
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(base::DeleteFile(prefs_file_, false));
    InProcessBrowserTest::TearDown();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceFirstRun);
    EXPECT_EQ(first_run::AUTO_IMPORT_NONE, first_run::auto_import_state());

    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

  void SetMasterPreferencesForTest(const char text[]) {
    text_.reset(new std::string(text));
  }

 private:
  base::FilePath prefs_file_;
  scoped_ptr<std::string> text_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsBrowserTestBase);
};

template<const char Text[]>
class FirstRunMasterPrefsBrowserTestT
    : public FirstRunMasterPrefsBrowserTestBase {
 public:
  FirstRunMasterPrefsBrowserTestT() {}

 protected:
  virtual void SetUp() OVERRIDE {
    SetMasterPreferencesForTest(Text);
    FirstRunMasterPrefsBrowserTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsBrowserTestT);
};

// Returns the true expected import state, derived from the original
// |expected_import_state|, for the current test machine's configuration. Some
// bot configurations do not have another profile (browser) to import from and
// thus the import must not be expected to have occurred.
int MaskExpectedImportState(int expected_import_state) {
  scoped_refptr<ImporterList> importer_list(new ImporterList());
  importer_list->DetectSourceProfilesHack(
      g_browser_process->GetApplicationLocale(), false);
  int source_profile_count = importer_list->count();
#if defined(OS_WIN)
  // On Windows, the importer's DetectIEProfiles() will always add to the count.
  // Internet Explorer always exists and always has something to import.
  EXPECT_GT(source_profile_count, 0);
#endif
  if (source_profile_count == 0)
    return expected_import_state & ~first_run::AUTO_IMPORT_PROFILE_IMPORTED;

  return expected_import_state;
}

}  // namespace

extern const char kImportDefault[] =
    "{\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportDefault>
    FirstRunMasterPrefsImportDefault;
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportDefault, ImportDefault) {
  int auto_import_state = first_run::auto_import_state();
  EXPECT_EQ(MaskExpectedImportState(first_run::AUTO_IMPORT_CALLED |
                                    first_run::AUTO_IMPORT_PROFILE_IMPORTED),
            auto_import_state);
}

// The bookmarks file doesn't actually need to exist for this integration test
// to trigger the interaction being tested.
extern const char kImportBookmarksFile[] =
    "{\n"
    "  \"distribution\": {\n"
    "     \"import_bookmarks_from_file\": \"/foo/doesntexists.wtv\"\n"
    "  }\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportBookmarksFile>
    FirstRunMasterPrefsImportBookmarksFile;
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportBookmarksFile,
                       ImportBookmarksFile) {
  int auto_import_state = first_run::auto_import_state();
  EXPECT_EQ(
      MaskExpectedImportState(first_run::AUTO_IMPORT_CALLED |
                              first_run::AUTO_IMPORT_PROFILE_IMPORTED |
                              first_run::AUTO_IMPORT_BOOKMARKS_FILE_IMPORTED),
      auto_import_state);
}

// Test an import with all import options disabled. This is a regression test
// for http://crbug.com/169984 where this would cause the import process to
// stay running, and the NTP to be loaded with no apps.
extern const char kImportNothing[] =
    "{\n"
    "  \"distribution\": {\n"
    "    \"import_bookmarks\": false,\n"
    "    \"import_history\": false,\n"
    "    \"import_home_page\": false,\n"
    "    \"import_search_engine\": false\n"
    "  }\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportNothing>
    FirstRunMasterPrefsImportNothing;
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportNothing,
                       ImportNothingAndShowNewTabPage) {
  EXPECT_EQ(first_run::AUTO_IMPORT_CALLED, first_run::auto_import_state());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(1, tab->GetMaxPageID());
}

#endif  // !defined(OS_CHROMEOS)
