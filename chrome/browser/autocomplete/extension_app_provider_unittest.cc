// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/extension_app_provider.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionAppProviderTest : public testing::Test {
 protected:
  struct test_data {
    const string16 input;
    const size_t num_results;
    const GURL output[3];
  };

  ExtensionAppProviderTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        history_service_(NULL) { }
  virtual ~ExtensionAppProviderTest() { }

  virtual void SetUp() OVERRIDE;

  void RunTest(test_data* keyword_cases,
               int num_cases);

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<ExtensionAppProvider> app_provider_;
  scoped_ptr<TestingProfile> profile_;
  HistoryService* history_service_;
};

void ExtensionAppProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateHistoryService(true, false);
  profile_->BlockUntilHistoryProcessesPendingRequests();
  history_service_ =
      HistoryServiceFactory::GetForProfile(profile_.get(),
                                           Profile::EXPLICIT_ACCESS);

  app_provider_ = new ExtensionAppProvider(NULL, profile_.get());

  struct TestExtensionApp {
    const char* app_name;
    const char* launch_url;
    bool should_match_against_launch_url;
    const char* title;
    int typed_count;
  } kExtensionApps[] = {
    {"COYB", "http://asdf/",            true,  "COYB", 7},
    {"NSNO", "http://fdsa/",            true,  "NSNO", 2},
    {"APPP", "chrome-extension://xyz/", false, "APPP", 2},
  };

  history::URLDatabase* url_db = history_service_->InMemoryDatabase();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExtensionApps); ++i) {
    // Populate the Extension Apps list.
    ExtensionAppProvider::ExtensionApp extension_app = {
      ASCIIToUTF16(kExtensionApps[i].app_name),
      ASCIIToUTF16(kExtensionApps[i].launch_url),
      kExtensionApps[i].should_match_against_launch_url
    };
    app_provider_->AddExtensionAppForTesting(extension_app);

    // Populate the InMemoryDatabase.
    history::URLRow info(GURL(kExtensionApps[i].launch_url));
    info.set_title(UTF8ToUTF16(kExtensionApps[i].title));
    info.set_typed_count(kExtensionApps[i].typed_count);
    url_db->AddURL(info);
  }
}

void ExtensionAppProviderTest::RunTest(
    test_data* keyword_cases,
    int num_cases) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(keyword_cases[i].input, string16::npos, string16(),
                            GURL(), true, false, true,
                            AutocompleteInput::ALL_MATCHES);
    app_provider_->Start(input, false);
    EXPECT_TRUE(app_provider_->done());
    matches = app_provider_->matches();
    EXPECT_EQ(keyword_cases[i].num_results, matches.size())
        << ASCIIToUTF16("Input was: ") + keyword_cases[i].input;
    if (matches.size() == keyword_cases[i].num_results) {
      for (size_t j = 0; j < keyword_cases[i].num_results; ++j)
        EXPECT_EQ(keyword_cases[i].output[j], matches[j].destination_url);
    }
  }
}

TEST_F(ExtensionAppProviderTest, BasicMatching) {
  test_data edit_cases[] = {
    // Searching for a nonexistent value should give nothing.
    {ASCIIToUTF16("Not Found"),       0, { GURL() }},

    // The letter 'o' appears in both extension apps.
    {ASCIIToUTF16("o"),               2, { GURL("http://asdf/"),
                                           GURL("http://fdsa/") }},
    // The string 'co' appears in one extension app.
    {ASCIIToUTF16("co"),              1, { GURL("http://asdf/") }},
    // Try with URL matching.
    {ASCIIToUTF16("http://asdf/"),    1, { GURL("http://asdf/") }},
    {ASCIIToUTF16("http://fdsa/"),    1, { GURL("http://fdsa/") }},

    // "xyz" appears in a launch URL, but we're not matching against it.
    {ASCIIToUTF16("xyz"),             0, { GURL() }},

    // But it should be matcheable by title.
    {ASCIIToUTF16("APPP"),            1, { GURL("chrome-extension://xyz/") }},

  };

  RunTest(edit_cases, ARRAYSIZE_UNSAFE(edit_cases));
}

TEST_F(ExtensionAppProviderTest, CreateMatchSanitize) {
  struct TestData {
    const char* name;
    const char* match_contents;
  } cases[] = {
    { "Test", "Test" },
    { "Test \n Test", "Test  Test" },
    { "Test\r\t\nTest", "TestTest" },
  };

  AutocompleteInput input(ASCIIToUTF16("Test"), string16::npos, string16(),
                          GURL(), true, true, true,
                          AutocompleteInput::BEST_MATCH);
  string16 url(ASCIIToUTF16("http://example.com"));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    ExtensionAppProvider::ExtensionApp extension_app =
        {ASCIIToUTF16(cases[i].name), url, true};
    AutocompleteMatch match =
        app_provider_->CreateAutocompleteMatch(input,
                                               extension_app,
                                               0,
                                               string16::npos);
    EXPECT_EQ(ASCIIToUTF16(cases[i].match_contents), match.contents);
  }
}
