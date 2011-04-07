// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/extension_app_provider.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/test/testing_profile.h"

class ExtensionAppProviderTest : public TestingBrowserProcessTest {
 protected:
  struct test_data {
    const string16 input;
    const size_t num_results;
    const GURL output[3];
  };

  ExtensionAppProviderTest() : history_service_(NULL) { }
  virtual ~ExtensionAppProviderTest() { }

  virtual void SetUp() OVERRIDE;

  void RunTest(test_data* keyword_cases,
               int num_cases);

 protected:
  MessageLoopForUI message_loop_;
  scoped_refptr<ExtensionAppProvider> app_provider_;
  scoped_ptr<TestingProfile> profile_;
  HistoryService* history_service_;
};

void ExtensionAppProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateHistoryService(true, false);
  profile_->BlockUntilHistoryProcessesPendingRequests();
  history_service_ = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);

  app_provider_ = new ExtensionAppProvider(NULL, profile_.get());

  struct ExtensionApps {
    std::string app_name;
    std::string url;
    std::string title;
    int typed_count;
  } kExtensionApps[] = {
    {"COYB", "http://asdf/", "COYB", 7},
    {"NSNO", "http://fdsa/", "NSNO", 2},
  };

  history::URLDatabase* url_db = history_service_->InMemoryDatabase();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExtensionApps); ++i) {
    // Populate the Extension Apps list.
    app_provider_->AddExtensionAppForTesting(kExtensionApps[i].app_name,
                                            kExtensionApps[i].url);

    // Populate the InMemoryDatabase.
    history::URLRow info(GURL(kExtensionApps[i].url));
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
    AutocompleteInput input(keyword_cases[i].input, string16(), true,
                            false, true, AutocompleteInput::ALL_MATCHES);
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
  };

  RunTest(edit_cases, ARRAYSIZE_UNSAFE(edit_cases));
}
