// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/desktop_search_win.h"
#include "components/search_engines/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class StartupBrowserCreatorWinTest : public testing::Test {
 public:
  StartupBrowserCreatorWinTest() {}

 protected:
  void SetWindowsDesktopSearchFeatureEnabled(bool enabled) {
    base::FeatureList::ClearInstanceForTesting();
    scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
    if (enabled) {
      feature_list->InitializeFromCommandLine(
          kWindowsDesktopSearchRedirectionFeature.name, std::string());
    }
    base::FeatureList::SetInstance(std::move(feature_list));
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorWinTest);
};

TEST_F(StartupBrowserCreatorWinTest,
       GetURLsFromCommandLineWithDesktopSearchURL) {
  const char kDesktopSearchURL[] =
      "https://www.bing.com/search?q=keyword&form=WNSGPH";

  TestingProfile profile;
  TemplateURLServiceFactoryTestUtil template_url_service_factory_test_util(
      &profile);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendArg(kDesktopSearchURL);

  // Expected vectors of URLs.
  const std::vector<GURL> desktop_search_url_vector({GURL(kDesktopSearchURL)});
  const std::vector<GURL> default_search_url_vector(
      {GetDefaultSearchURLForSearchTerms(
          TemplateURLServiceFactory::GetForProfile(&profile), L"keyword")});

  // Preference unset, feature enabled.
  SetWindowsDesktopSearchFeatureEnabled(true);
  EXPECT_EQ(desktop_search_url_vector,
            StartupBrowserCreator::GetURLsFromCommandLine(
                command_line, base::FilePath(), &profile));

  // Preference set to disabled, feature enabled.
  profile.GetPrefs()->SetBoolean(prefs::kWindowsDesktopSearchRedirectionPref,
                                 false);
  SetWindowsDesktopSearchFeatureEnabled(true);
  EXPECT_EQ(desktop_search_url_vector,
            StartupBrowserCreator::GetURLsFromCommandLine(
                command_line, base::FilePath(), &profile));

  // Preference set to enabled, feature enabled.
  profile.GetPrefs()->SetBoolean(prefs::kWindowsDesktopSearchRedirectionPref,
                                 true);
  SetWindowsDesktopSearchFeatureEnabled(true);
  EXPECT_EQ(default_search_url_vector,
            StartupBrowserCreator::GetURLsFromCommandLine(
                command_line, base::FilePath(), &profile));

  // Preference set to enabled, feature disabled.
  profile.GetPrefs()->SetBoolean(prefs::kWindowsDesktopSearchRedirectionPref,
                                 true);
  SetWindowsDesktopSearchFeatureEnabled(false);
  EXPECT_EQ(desktop_search_url_vector,
            StartupBrowserCreator::GetURLsFromCommandLine(
                command_line, base::FilePath(), &profile));
}
