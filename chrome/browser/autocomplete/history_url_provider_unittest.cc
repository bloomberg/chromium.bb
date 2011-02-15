// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/history/history.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

struct TestURLInfo {
  std::string url;
  std::string title;
  int visit_count;
  int typed_count;
} test_db[] = {
  {"http://www.google.com/", "Google", 3, 3},

  // High-quality pages should get a host synthesized as a lower-quality match.
  {"http://slashdot.org/favorite_page.html", "Favorite page", 200, 100},

  // Less popular pages should have hosts synthesized as higher-quality
  // matches.
  {"http://kerneltrap.org/not_very_popular.html", "Less popular", 4, 0},

  // Unpopular pages should not appear in the results at all.
  {"http://freshmeat.net/unpopular.html", "Unpopular", 1, 1},

  // If a host has a match, we should pick it up during host synthesis.
  {"http://news.google.com/?ned=us&topic=n", "Google News - U.S.", 2, 2},
  {"http://news.google.com/", "Google News", 1, 1},

  // Suggested short URLs must be "good enough" and must match user input.
  {"http://foo.com/", "Dir", 5, 5},
  {"http://foo.com/dir/", "Dir", 2, 2},
  {"http://foo.com/dir/another/", "Dir", 5, 1},
  {"http://foo.com/dir/another/again/", "Dir", 10, 0},
  {"http://foo.com/dir/another/again/myfile.html", "File", 10, 2},

  // We throw in a lot of extra URLs here to make sure we're testing the
  // history database's query, not just the autocomplete provider.
  {"http://startest.com/y/a", "A", 2, 2},
  {"http://startest.com/y/b", "B", 5, 2},
  {"http://startest.com/x/c", "C", 5, 2},
  {"http://startest.com/x/d", "D", 5, 5},
  {"http://startest.com/y/e", "E", 4, 2},
  {"http://startest.com/y/f", "F", 3, 2},
  {"http://startest.com/y/g", "G", 3, 2},
  {"http://startest.com/y/h", "H", 3, 2},
  {"http://startest.com/y/i", "I", 3, 2},
  {"http://startest.com/y/j", "J", 3, 2},
  {"http://startest.com/y/k", "K", 3, 2},
  {"http://startest.com/y/l", "L", 3, 2},
  {"http://startest.com/y/m", "M", 3, 2},

  // A file: URL is useful for testing that fixup does the right thing w.r.t.
  // the number of trailing slashes on the user's input.
  {"file:///C:/foo.txt", "", 2, 2},

  // Results with absurdly high typed_counts so that very generic queries like
  // "http" will give consistent results even if more data is added above.
  {"http://bogussite.com/a", "Bogus A", 10002, 10000},
  {"http://bogussite.com/b", "Bogus B", 10001, 10000},
  {"http://bogussite.com/c", "Bogus C", 10000, 10000},

  // Domain name with number.
  {"http://www.17173.com/", "Domain with number", 3, 3},

  // URLs to test exact-matching behavior.
  {"http://go/", "Intranet URL", 1, 1},
  {"http://gooey/", "Intranet URL 2", 5, 5},

  // URLs for testing offset adjustment.
  {"http://www.\xEA\xB5\x90\xEC\x9C\xA1.kr/", "Korean", 2, 2},
  {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2},
  {"http://ms/c++%20style%20guide", "Style guide", 2, 2},

  // URLs for testing ctrl-enter behavior.
  {"http://binky/", "Intranet binky", 2, 2},
  {"http://winky/", "Intranet winky", 2, 2},
  {"http://www.winky.com/", "Internet winky", 5, 0},
};

class HistoryURLProviderTest : public TestingBrowserProcessTest,
                               public ACProviderListener {
 public:
  HistoryURLProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}

  // ACProviderListener
  virtual void OnProviderUpdate(bool updated_matches);

 protected:
  // testing::Test
  virtual void SetUp() {
    SetUpImpl(false);
  }
  virtual void TearDown();

  // Does the real setup.
  void SetUpImpl(bool no_db);

  // Fills test data into the history system.
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided.
  void RunTest(const string16 text,
               const string16& desired_tld,
               bool prevent_inline_autocomplete,
               const std::string* expected_urls,
               size_t num_results);

  void RunAdjustOffsetTest(const string16 text, size_t expected_offset);

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  ACMatches matches_;
  scoped_ptr<TestingProfile> profile_;
  HistoryService* history_service_;

 private:
  scoped_refptr<HistoryURLProvider> autocomplete_;
};

class HistoryURLProviderTestNoDB : public HistoryURLProviderTest {
 protected:
  virtual void SetUp() {
    SetUpImpl(true);
  }
};

void HistoryURLProviderTest::OnProviderUpdate(bool updated_matches) {
  if (autocomplete_->done())
    MessageLoop::current()->Quit();
}

void HistoryURLProviderTest::SetUpImpl(bool no_db) {
  profile_.reset(new TestingProfile());
  profile_->CreateHistoryService(true, no_db);
  history_service_ = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);

  autocomplete_ = new HistoryURLProvider(this, profile_.get(), "en-US,en,ko");

  FillData();
}

void HistoryURLProviderTest::TearDown() {
  autocomplete_ = NULL;
}

void HistoryURLProviderTest::FillData() {
  // All visits are a long time ago (some tests require this since we do some
  // special logic for things visited very recently). Note that this time must
  // be more recent than the "archived history" threshold for the data to go
  // into the main database.
  //
  // TODO(brettw) It would be nice if we could test this behavior, in which
  // case the time would be specifed in the test_db structure.
  Time visit_time = Time::Now() - TimeDelta::FromDays(80);

  for (size_t i = 0; i < arraysize(test_db); ++i) {
    const TestURLInfo& cur = test_db[i];
    const GURL current_url(cur.url);
    history_service_->AddPageWithDetails(current_url, UTF8ToUTF16(cur.title),
                                         cur.visit_count, cur.typed_count,
                                         visit_time, false,
                                         history::SOURCE_BROWSED);
  }
}

void HistoryURLProviderTest::RunTest(const string16 text,
                                     const string16& desired_tld,
                                     bool prevent_inline_autocomplete,
                                     const std::string* expected_urls,
                                     size_t num_results) {
  AutocompleteInput input(text, desired_tld, prevent_inline_autocomplete,
                          false, true, false);
  autocomplete_->Start(input, false);
  if (!autocomplete_->done())
    MessageLoop::current()->Run();

  matches_ = autocomplete_->matches();
  ASSERT_EQ(num_results, matches_.size()) << "Input text: " << text
                                          << "\nTLD: \"" << desired_tld << "\"";
  for (size_t i = 0; i < num_results; ++i)
    EXPECT_EQ(expected_urls[i], matches_[i].destination_url.spec());
}

void HistoryURLProviderTest::RunAdjustOffsetTest(const string16 text,
                                                 size_t expected_offset) {
  AutocompleteInput input(text, string16(), false, false, true, false);
  autocomplete_->Start(input, false);
  if (!autocomplete_->done())
    MessageLoop::current()->Run();

  matches_ = autocomplete_->matches();
  ASSERT_GE(matches_.size(), 1U) << "Input text: " << text;
  EXPECT_EQ(expected_offset, matches_[0].inline_autocomplete_offset);
}

TEST_F(HistoryURLProviderTest, PromoteShorterURLs) {
  // Test that hosts get synthesized below popular pages.
  const std::string expected_nonsynth[] = {
    "http://slashdot.org/favorite_page.html",
    "http://slashdot.org/",
  };
  RunTest(ASCIIToUTF16("slash"), string16(), true, expected_nonsynth,
          arraysize(expected_nonsynth));

  // Test that hosts get synthesized above less popular pages.
  const std::string expected_synth[] = {
    "http://kerneltrap.org/",
    "http://kerneltrap.org/not_very_popular.html",
  };
  RunTest(ASCIIToUTF16("kernel"), string16(), true, expected_synth,
          arraysize(expected_synth));

  // Test that unpopular pages are ignored completely.
  RunTest(ASCIIToUTF16("fresh"), string16(), true, NULL, 0);

  // Test that if we have a synthesized host that matches a suggestion, they
  // get combined into one.
  const std::string expected_combine[] = {
    "http://news.google.com/",
    "http://news.google.com/?ned=us&topic=n",
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("news"), string16(), true,
      expected_combine, arraysize(expected_combine)));
  // The title should also have gotten set properly on the host for the
  // synthesized one, since it was also in the results.
  EXPECT_EQ(ASCIIToUTF16("Google News"), matches_.front().description);

  // Test that short URL matching works correctly as the user types more
  // (several tests):
  // The entry for foo.com is the best of all five foo.com* entries.
  const std::string short_1[] = {
    "http://foo.com/",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/",
  };
  RunTest(ASCIIToUTF16("foo"), string16(), true, short_1, arraysize(short_1));

  // When the user types the whole host, make sure we don't get two results for
  // it.
  const std::string short_2[] = {
    "http://foo.com/",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/",
    "http://foo.com/dir/another/",
  };
  RunTest(ASCIIToUTF16("foo.com"), string16(), true, short_2,
          arraysize(short_2));
  RunTest(ASCIIToUTF16("foo.com/"), string16(), true, short_2,
          arraysize(short_2));

  // The filename is the second best of the foo.com* entries, but there is a
  // shorter URL that's "good enough".  The host doesn't match the user input
  // and so should not appear.
  const std::string short_3[] = {
    "http://foo.com/d",
    "http://foo.com/dir/another/",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/",
  };
  RunTest(ASCIIToUTF16("foo.com/d"), string16(), true, short_3,
          arraysize(short_3));

  // We shouldn't promote shorter URLs than the best if they're not good
  // enough.
  const std::string short_4[] = {
    "http://foo.com/dir/another/a",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/another/again/",
  };
  RunTest(ASCIIToUTF16("foo.com/dir/another/a"), string16(), true, short_4,
          arraysize(short_4));

  // Exact matches should always be best no matter how much more another match
  // has been typed.
  const std::string short_5a[] = {
    "http://gooey/",
    "http://www.google.com/",
  };
  const std::string short_5b[] = {
    "http://go/",
    "http://gooey/",
    "http://www.google.com/",
  };
  RunTest(ASCIIToUTF16("g"), string16(), false, short_5a, arraysize(short_5a));
  RunTest(ASCIIToUTF16("go"), string16(), false, short_5b, arraysize(short_5b));
}

TEST_F(HistoryURLProviderTest, CullRedirects) {
  // URLs we will be using, plus the visit counts they will initially get
  // (the redirect set below will also increment the visit counts). We want
  // the results to be in A,B,C order. Note also that our visit counts are
  // all high enough so that domain synthesizing won't get triggered.
  struct RedirectCase {
    const char* url;
    int count;
  };
  static const RedirectCase redirect[] = {
    {"http://redirects/A", 30},
    {"http://redirects/B", 20},
    {"http://redirects/C", 10}
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(redirect); i++) {
    history_service_->AddPageWithDetails(GURL(redirect[i].url),
                                         UTF8ToUTF16("Title"),
                                         redirect[i].count, redirect[i].count,
                                         Time::Now(), false,
                                         history::SOURCE_BROWSED);
  }

  // Create a B->C->A redirect chain, but set the visit counts such that they
  // will appear in A,B,C order in the results. The autocomplete query will
  // search for the most recent visit when looking for redirects, so this will
  // be found even though the previous visits had no redirects.
  history::RedirectList redirects_to_a;
  redirects_to_a.push_back(GURL(redirect[1].url));
  redirects_to_a.push_back(GURL(redirect[2].url));
  redirects_to_a.push_back(GURL(redirect[0].url));
  history_service_->AddPage(GURL(redirect[0].url), NULL, 0, GURL(),
                            PageTransition::TYPED, redirects_to_a,
                            history::SOURCE_BROWSED, true);

  // Because all the results are part of a redirect chain with other results,
  // all but the first one (A) should be culled. We should get the default
  // "what you typed" result, plus this one.
  const string16 typing(ASCIIToUTF16("http://redirects/"));
  const std::string expected_results[] = {
    UTF16ToUTF8(typing),
    redirect[0].url};
  RunTest(typing, string16(), true, expected_results,
          arraysize(expected_results));
}

TEST_F(HistoryURLProviderTest, WhatYouTyped) {
  // Make sure we suggest a What You Typed match at the right times.
  RunTest(ASCIIToUTF16("wytmatch"), string16(), false, NULL, 0);
  RunTest(ASCIIToUTF16("wytmatch foo bar"), string16(), false, NULL, 0);
  RunTest(ASCIIToUTF16("wytmatch+foo+bar"), string16(), false, NULL, 0);
  RunTest(ASCIIToUTF16("wytmatch+foo+bar.com"), string16(), false, NULL, 0);

  const std::string results_1[] = {"http://www.wytmatch.com/"};
  RunTest(ASCIIToUTF16("wytmatch"), ASCIIToUTF16("com"), false, results_1,
          arraysize(results_1));

  const std::string results_2[] = {"http://wytmatch%20foo%20bar/"};
  RunTest(ASCIIToUTF16("http://wytmatch foo bar"), string16(), false, results_2,
          arraysize(results_2));

  const std::string results_3[] = {"https://wytmatch%20foo%20bar/"};
  RunTest(ASCIIToUTF16("https://wytmatch foo bar"), string16(), false,
          results_3, arraysize(results_3));

  // Test the corner case where a user has fully typed a previously visited
  // intranet address and is now hitting ctrl-enter, which completes to a
  // previously unvisted internet domain.
  const std::string binky_results[] = {"http://binky/"};
  const std::string binky_com_results[] = {
    "http://www.binky.com/",
    "http://binky/",
  };
  RunTest(ASCIIToUTF16("binky"), string16(), false, binky_results,
          arraysize(binky_results));
  RunTest(ASCIIToUTF16("binky"), ASCIIToUTF16("com"), false, binky_com_results,
          arraysize(binky_com_results));

  // Test the related case where a user has fully typed a previously visited
  // intranet address and is now hitting ctrl-enter, which completes to a
  // previously visted internet domain.
  const std::string winky_results[] = {
    "http://winky/",
    "http://www.winky.com/",
  };
  const std::string winky_com_results[] = {
    "http://www.winky.com/",
    "http://winky/",
  };
  RunTest(ASCIIToUTF16("winky"), string16(), false, winky_results,
          arraysize(winky_results));
  RunTest(ASCIIToUTF16("winky"), ASCIIToUTF16("com"), false, winky_com_results,
          arraysize(winky_com_results));
}

TEST_F(HistoryURLProviderTest, Fixup) {
  // Test for various past crashes we've had.
  RunTest(ASCIIToUTF16("\\"), string16(), false, NULL, 0);
  RunTest(ASCIIToUTF16("#"), string16(), false, NULL, 0);
  RunTest(ASCIIToUTF16("%20"), string16(), false, NULL, 0);
  RunTest(WideToUTF16(L"\uff65@s"), string16(), false, NULL, 0);
  RunTest(WideToUTF16(L"\u2015\u2015@ \uff7c"), string16(), false, NULL, 0);

  // Fixing up "file:" should result in an inline autocomplete offset of just
  // after "file:", not just after "file://".
  const string16 input_1(ASCIIToUTF16("file:"));
  const std::string fixup_1[] = {"file:///C:/foo.txt"};
  ASSERT_NO_FATAL_FAILURE(RunTest(input_1, string16(), false, fixup_1,
                                  arraysize(fixup_1)));
  EXPECT_EQ(input_1.length(), matches_.front().inline_autocomplete_offset);

  // Fixing up "http:/" should result in an inline autocomplete offset of just
  // after "http:/", not just after "http:".
  const string16 input_2(ASCIIToUTF16("http:/"));
  const std::string fixup_2[] = {
    "http://bogussite.com/a",
    "http://bogussite.com/b",
    "http://bogussite.com/c",
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(input_2, string16(), false, fixup_2,
                                  arraysize(fixup_2)));
  EXPECT_EQ(input_2.length(), matches_.front().inline_autocomplete_offset);

  // Adding a TLD to a small number like "56" should result in "www.56.com"
  // rather than "0.0.0.56.com".
  const std::string fixup_3[] = {"http://www.56.com/"};
  RunTest(ASCIIToUTF16("56"), ASCIIToUTF16("com"), true, fixup_3,
          arraysize(fixup_3));

  // An input looks like a IP address like "127.0.0.1" should result in
  // "http://127.0.0.1/".
  const std::string fixup_4[] = {"http://127.0.0.1/"};
  RunTest(ASCIIToUTF16("127.0.0.1"), string16(), false, fixup_4,
          arraysize(fixup_4));

  // An number "17173" should result in "http://www.17173.com/" in db.
  const std::string fixup_5[] = {"http://www.17173.com/"};
  RunTest(ASCIIToUTF16("17173"), string16(), false, fixup_5,
          arraysize(fixup_5));
}

TEST_F(HistoryURLProviderTest, AdjustOffset) {
  RunAdjustOffsetTest(WideToUTF16(L"http://www.\uAD50\uC721"), 13);
  RunAdjustOffsetTest(ASCIIToUTF16("http://spaces.com/path%20with%20spa"), 31);
  RunAdjustOffsetTest(ASCIIToUTF16("http://ms/c++ s"), 15);
}

TEST_F(HistoryURLProviderTestNoDB, NavigateWithoutDB) {
  // Ensure that we will still produce matches for navigation when there is no
  // database.
  std::string navigation_1[] = {"http://test.com/"};
  RunTest(ASCIIToUTF16("test.com"), string16(), false, navigation_1,
          arraysize(navigation_1));

  std::string navigation_2[] = {"http://slash/"};
  RunTest(ASCIIToUTF16("slash"), string16(), false, navigation_2,
          arraysize(navigation_2));

  RunTest(ASCIIToUTF16("this is a query"), string16(), false, NULL, 0);
}
