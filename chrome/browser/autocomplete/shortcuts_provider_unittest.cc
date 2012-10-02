// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_provider.h"

#include <math.h>

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/shortcuts_backend_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using history::ShortcutsBackend;

namespace {

struct TestShortcutInfo {
  std::string guid;
  std::string url;
  std::string title;  // The text that orginally was searched for.
  std::string contents;
  std::string contents_class;
  std::string description;
  std::string description_class;
  int typed_count;
  int days_from_now;
} shortcut_test_db[] = {
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E0",
    "http://www.google.com/", "goog",
    "Google", "0,1,4,0", "Google", "0,3,4,1", 100, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E1",
    "http://slashdot.org/", "slash",
    "slashdot.org", "0,3,5,1",
    "Slashdot - News for nerds, stuff that matters", "0,2,5,0", 100, 0},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E2",
    "http://slashdot.org/", "news",
    "slashdot.org", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0", 5, 0},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E3",
    "http://sports.yahoo.com/", "news",
    "sports.yahoo.com", "0,1",
    "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
    "0,0,23,2,27,0", 5, 2},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E4",
    "http://www.cnn.com/index.html", "news weather",
    "www.cnn.com/index.html", "0,1",
    "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
    "0,0,19,2,23,0,38,2,45,0", 10, 1},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E5",
    "http://sports.yahoo.com/", "nhl scores",
    "sports.yahoo.com", "0,1",
    "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
    "0,0,29,2,35,0", 10, 1},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E6",
    "http://www.nhl.com/scores/index.html", "nhl scores",
    "www.nhl.com/scores/index.html", "0,1,4,3,7,1",
    "January 13, 2010 - NHL.com - Scores", "0,0,19,2,22,0,29,2,35,0", 1, 5},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E7",
    "http://www.testsite.com/a.html", "just",
    "www.testsite.com/a.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 5},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E8",
    "http://www.testsite.com/b.html", "just",
    "www.testsite.com/b.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 2, 5},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E9",
    "http://www.testsite.com/c.html", "just",
    "www.testsite.com/c.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 8},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EA",
    "http://www.testsite.com/d.html", "just a",
    "www.testsite.com/d.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 12},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EB",
    "http://www.testsite.com/e.html", "just a t",
    "www.testsite.com/e.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 12},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EC",
    "http://www.testsite.com/f.html", "just a te",
    "www.testsite.com/f.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 12},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880ED",
    "http://www.daysagotest.com/a.html", "ago",
    "www.daysagotest.com/a.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 1},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EE",
    "http://www.daysagotest.com/b.html", "ago",
    "www.daysagotest.com/b.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 2},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EF",
    "http://www.daysagotest.com/c.html", "ago",
    "www.daysagotest.com/c.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 3},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880F0",
    "http://www.daysagotest.com/d.html", "ago",
    "www.daysagotest.com/d.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 4},
};

}  // namespace

class ShortcutsProviderTest : public testing::Test,
                              public AutocompleteProviderListener {
 public:
  ShortcutsProviderTest();

  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE;

 protected:
  class SetShouldContain
      : public std::unary_function<const std::string&, std::set<std::string> > {
   public:
    explicit SetShouldContain(const ACMatches& matched_urls);

    void operator()(const std::string& expected);
    std::set<std::string> Leftovers() const { return matches_; }

   private:
    std::set<std::string> matches_;
  };

  void SetUp();
  void TearDown();

  // Fills test data into the provider.
  void FillData(TestShortcutInfo* db, size_t db_size);

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const string16 text,
               std::vector<std::string> expected_urls,
               std::string expected_top_result);

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestingProfile profile_;

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

  scoped_refptr<ShortcutsBackend> backend_;
  scoped_refptr<ShortcutsProvider> provider_;
};

ShortcutsProviderTest::ShortcutsProviderTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

void ShortcutsProviderTest::OnProviderUpdate(bool updated_matches) {}

void ShortcutsProviderTest::SetUp() {
  ShortcutsBackendFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting);
  backend_ = ShortcutsBackendFactory::GetForProfile(&profile_);
  ASSERT_TRUE(backend_.get());
  profile_.CreateHistoryService(true, false);
  provider_ = new ShortcutsProvider(this, &profile_);
  FillData(shortcut_test_db, arraysize(shortcut_test_db));
}

void ShortcutsProviderTest::TearDown() {
  // Run all pending tasks or else some threads hold on to the message loop
  // and prevent it from being deleted.
  message_loop_.RunAllPending();
  provider_ = NULL;
}

void ShortcutsProviderTest::FillData(TestShortcutInfo* db, size_t db_size) {
  DCHECK(provider_.get());
  size_t expected_size = backend_->shortcuts_map().size() + db_size;
  for (size_t i = 0; i < db_size; ++i) {
    const TestShortcutInfo& cur = db[i];
    ShortcutsBackend::Shortcut shortcut(cur.guid,
        ASCIIToUTF16(cur.title), GURL(cur.url), ASCIIToUTF16(cur.contents),
        AutocompleteMatch::ClassificationsFromString(cur.contents_class),
        ASCIIToUTF16(cur.description),
        AutocompleteMatch::ClassificationsFromString(cur.description_class),
        base::Time::Now() - base::TimeDelta::FromDays(cur.days_from_now),
        cur.typed_count);
    backend_->AddShortcut(shortcut);
  }
  EXPECT_EQ(expected_size, backend_->shortcuts_map().size());
}

ShortcutsProviderTest::SetShouldContain::SetShouldContain(
    const ACMatches& matched_urls) {
  for (ACMatches::const_iterator iter = matched_urls.begin();
       iter != matched_urls.end(); ++iter)
    matches_.insert(iter->destination_url.spec());
}

void ShortcutsProviderTest::SetShouldContain::operator()(
    const std::string& expected) {
  EXPECT_EQ(1U, matches_.erase(expected));
}

void ShortcutsProviderTest::RunTest(const string16 text,
                                   std::vector<std::string> expected_urls,
                                   std::string expected_top_result) {
  std::sort(expected_urls.begin(), expected_urls.end());

  MessageLoop::current()->RunAllPending();
  AutocompleteInput input(text, string16(), false, false, true,
                          AutocompleteInput::ALL_MATCHES);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());

  ac_matches_ = provider_->matches();

  // We should have gotten back at most AutocompleteProvider::kMaxMatches.
  EXPECT_LE(ac_matches_.size(), AutocompleteProvider::kMaxMatches);

  // If the number of expected and actual matches aren't equal then we need
  // test no further, but let's do anyway so that we know which URLs failed.
  EXPECT_EQ(expected_urls.size(), ac_matches_.size());

  // Verify that all expected URLs were found and that all found URLs
  // were expected.
  std::set<std::string> Leftovers =
      for_each(expected_urls.begin(), expected_urls.end(),
               SetShouldContain(ac_matches_)).Leftovers();
  EXPECT_EQ(0U, Leftovers.size());

  // See if we got the expected top scorer.
  if (!ac_matches_.empty()) {
    std::partial_sort(ac_matches_.begin(), ac_matches_.begin() + 1,
                      ac_matches_.end(), AutocompleteMatch::MoreRelevant);
    EXPECT_EQ(expected_top_result, ac_matches_[0].destination_url.spec());
  }
}

TEST_F(ShortcutsProviderTest, SimpleSingleMatch) {
  string16 text(ASCIIToUTF16("go"));
  std::string expected_url("http://www.google.com/");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(text, expected_urls, expected_url);
}

TEST_F(ShortcutsProviderTest, MultiMatch) {
  string16 text(ASCIIToUTF16("NEWS"));
  std::vector<std::string> expected_urls;
  // Scores high because of completion length.
  expected_urls.push_back("http://slashdot.org/");
  // Scores high because of visit count.
  expected_urls.push_back("http://sports.yahoo.com/");
  // Scores high because of visit count but less match span,
  // which is more important.
  expected_urls.push_back("http://www.cnn.com/index.html");
  RunTest(text, expected_urls, "http://slashdot.org/");
}

TEST_F(ShortcutsProviderTest, TypedCountMatches) {
  string16 text(ASCIIToUTF16("just"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.testsite.com/b.html");
  expected_urls.push_back("http://www.testsite.com/a.html");
  expected_urls.push_back("http://www.testsite.com/c.html");
  RunTest(text, expected_urls, "http://www.testsite.com/b.html");
}

TEST_F(ShortcutsProviderTest, FragmentLengthMatches) {
  string16 text(ASCIIToUTF16("just a"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.testsite.com/d.html");
  expected_urls.push_back("http://www.testsite.com/e.html");
  expected_urls.push_back("http://www.testsite.com/f.html");
  RunTest(text, expected_urls, "http://www.testsite.com/d.html");
}

TEST_F(ShortcutsProviderTest, DaysAgoMatches) {
  string16 text(ASCIIToUTF16("ago"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.daysagotest.com/a.html");
  expected_urls.push_back("http://www.daysagotest.com/b.html");
  expected_urls.push_back("http://www.daysagotest.com/c.html");
  RunTest(text, expected_urls, "http://www.daysagotest.com/a.html");
}

// Helper class to make running tests of ClassifyAllMatchesInString() more
// convenient.
class ClassifyTest {
 public:
  ClassifyTest(const string16& text, ACMatchClassifications matches);
  ~ClassifyTest();

  ACMatchClassifications RunTest(const string16& find_text);

 private:
  const string16 text_;
  const ACMatchClassifications matches_;
};

ClassifyTest::ClassifyTest(const string16& text, ACMatchClassifications matches)
    : text_(text),
      matches_(matches) {
}

ClassifyTest::~ClassifyTest() {
}

ACMatchClassifications ClassifyTest::RunTest(const string16& find_text) {
  return ShortcutsProvider::ClassifyAllMatchesInString(find_text,
      ShortcutsProvider::CreateWordMapForString(find_text), text_, matches_);
}

TEST_F(ShortcutsProviderTest, ClassifyAllMatchesInString) {
  ACMatchClassifications matches;
  matches.push_back(ACMatchClassification(0, ACMatchClassification::NONE));
  ClassifyTest classify_test(ASCIIToUTF16("A man, a plan, a canal Panama"),
                             matches);

  ACMatchClassifications spans_a = classify_test.RunTest(ASCIIToUTF16("man"));
  // ACMatch spans should be: '--MMM------------------------'
  ASSERT_EQ(3U, spans_a.size());
  EXPECT_EQ(0U, spans_a[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[0].style);
  EXPECT_EQ(2U, spans_a[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[1].style);
  EXPECT_EQ(5U, spans_a[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[2].style);

  ACMatchClassifications spans_b = classify_test.RunTest(ASCIIToUTF16("man p"));
  // ACMatch spans should be: '--MMM----M-------------M-----'
  ASSERT_EQ(7U, spans_b.size());
  EXPECT_EQ(0U, spans_b[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[0].style);
  EXPECT_EQ(2U, spans_b[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_b[1].style);
  EXPECT_EQ(5U, spans_b[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[2].style);
  EXPECT_EQ(9U, spans_b[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_b[3].style);
  EXPECT_EQ(10U, spans_b[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[4].style);
  EXPECT_EQ(23U, spans_b[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_b[5].style);
  EXPECT_EQ(24U, spans_b[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[6].style);

  ACMatchClassifications spans_c =
      classify_test.RunTest(ASCIIToUTF16("man plan panama"));
  // ACMatch spans should be:'--MMM----MMMM----------MMMMMM'
  ASSERT_EQ(6U, spans_c.size());
  EXPECT_EQ(0U, spans_c[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_c[0].style);
  EXPECT_EQ(2U, spans_c[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_c[1].style);
  EXPECT_EQ(5U, spans_c[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_c[2].style);
  EXPECT_EQ(9U, spans_c[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_c[3].style);
  EXPECT_EQ(13U, spans_c[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_c[4].style);
  EXPECT_EQ(23U, spans_c[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_c[5].style);

  ClassifyTest classify_test2(ASCIIToUTF16("Yahoo! Sports - Sports News, "
      "Scores, Rumors, Fantasy Games, and more"), matches);

  ACMatchClassifications spans_d = classify_test2.RunTest(ASCIIToUTF16("ne"));
  // ACMatch spans should match first two letters of the "news".
  ASSERT_EQ(3U, spans_d.size());
  EXPECT_EQ(0U, spans_d[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_d[0].style);
  EXPECT_EQ(23U, spans_d[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_d[1].style);
  EXPECT_EQ(25U, spans_d[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_d[2].style);

  ACMatchClassifications spans_e =
      classify_test2.RunTest(ASCIIToUTF16("news r"));
  // ACMatch spans should be the same as original matches.
  ASSERT_EQ(15U, spans_e.size());
  EXPECT_EQ(0U, spans_e[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[0].style);
  // "r" in "Sports".
  EXPECT_EQ(10U, spans_e[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[1].style);
  EXPECT_EQ(11U, spans_e[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[2].style);
  // "r" in second "Sports".
  EXPECT_EQ(19U, spans_e[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[3].style);
  EXPECT_EQ(20U, spans_e[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[4].style);
  // "News".
  EXPECT_EQ(23U, spans_e[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[5].style);
  EXPECT_EQ(27U, spans_e[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[6].style);
  // "r" in "Scores".
  EXPECT_EQ(32U, spans_e[7].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[7].style);
  EXPECT_EQ(33U, spans_e[8].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[8].style);
  // First "r" in "Rumors".
  EXPECT_EQ(37U, spans_e[9].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[9].style);
  EXPECT_EQ(38U, spans_e[10].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[10].style);
  // Second "r" in "Rumors".
  EXPECT_EQ(41U, spans_e[11].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[11].style);
  EXPECT_EQ(42U, spans_e[12].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[12].style);
  // "r" in "more".
  EXPECT_EQ(66U, spans_e[13].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[13].style);
  EXPECT_EQ(67U, spans_e[14].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[14].style);

  matches.clear();
  matches.push_back(ACMatchClassification(0, ACMatchClassification::URL));
  ClassifyTest classify_test3(ASCIIToUTF16("livescore.goal.com"), matches);

  ACMatchClassifications spans_f = classify_test3.RunTest(ASCIIToUTF16("go"));
  // ACMatch spans should match first two letters of the "goal".
  ASSERT_EQ(3U, spans_f.size());
  EXPECT_EQ(0U, spans_f[0].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_f[0].style);
  EXPECT_EQ(10U, spans_f[1].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_f[1].style);
  EXPECT_EQ(12U, spans_f[2].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_f[2].style);

  matches.clear();
  matches.push_back(ACMatchClassification(0, ACMatchClassification::NONE));
  matches.push_back(ACMatchClassification(13, ACMatchClassification::URL));
  ClassifyTest classify_test4(ASCIIToUTF16("Email login: mail.somecorp.com"),
                              matches);

  ACMatchClassifications spans_g = classify_test4.RunTest(ASCIIToUTF16("ail"));
  ASSERT_EQ(6U, spans_g.size());
  EXPECT_EQ(0U, spans_g[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_g[0].style);
  EXPECT_EQ(2U, spans_g[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_g[1].style);
  EXPECT_EQ(5U, spans_g[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_g[2].style);
  EXPECT_EQ(13U, spans_g[3].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_g[3].style);
  EXPECT_EQ(14U, spans_g[4].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_g[4].style);
  EXPECT_EQ(17U, spans_g[5].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_g[5].style);

  ACMatchClassifications spans_h =
      classify_test4.RunTest(ASCIIToUTF16("lo log"));
  ASSERT_EQ(4U, spans_h.size());
  EXPECT_EQ(0U, spans_h[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_h[0].style);
  EXPECT_EQ(6U, spans_h[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_h[1].style);
  EXPECT_EQ(9U, spans_h[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_h[2].style);
  EXPECT_EQ(13U, spans_h[3].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_h[3].style);

  ACMatchClassifications spans_i =
      classify_test4.RunTest(ASCIIToUTF16("ail em"));
  // 'Email' and 'ail' should be matched.
  ASSERT_EQ(5U, spans_i.size());
  EXPECT_EQ(0U, spans_i[0].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_i[0].style);
  EXPECT_EQ(5U, spans_i[1].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_i[1].style);
  EXPECT_EQ(13U, spans_i[2].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_i[2].style);
  EXPECT_EQ(14U, spans_i[3].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_i[3].style);
  EXPECT_EQ(17U, spans_i[4].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_i[4].style);

  // Some web sites do not have a description.  If the string being searched is
  // empty, the classifications must also be empty: http://crbug.com/148647
  // Extra parens in the next line hack around C++03's "most vexing parse".
  class ClassifyTest classify_test5((string16()), ACMatchClassifications());
  ACMatchClassifications spans_j = classify_test5.RunTest(ASCIIToUTF16("man"));
  ASSERT_EQ(0U, spans_j.size());

  // Matches which end at beginning of classification merge properly.
  matches.clear();
  matches.push_back(ACMatchClassification(0, ACMatchClassification::DIM));
  matches.push_back(ACMatchClassification(9, ACMatchClassification::NONE));
  ClassifyTest classify_test6(ASCIIToUTF16("html password example"), matches);

  // Extra space in the next string avoids having the string be a prefix of the
  // text above, which would allow for two different valid classification sets,
  // one of which uses two spans (the first of which would mark all of "html
  // pass" as a match) and one which uses four (which marks the individual words
  // as matches but not the space between them).  This way only the latter is
  // valid.
  ACMatchClassifications spans_k =
      classify_test6.RunTest(ASCIIToUTF16("html  pass"));
  ASSERT_EQ(4U, spans_k.size());
  EXPECT_EQ(0U, spans_k[0].offset);
  EXPECT_EQ(ACMatchClassification::DIM | ACMatchClassification::MATCH,
            spans_k[0].style);
  EXPECT_EQ(4U, spans_k[1].offset);
  EXPECT_EQ(ACMatchClassification::DIM, spans_k[1].style);
  EXPECT_EQ(5U, spans_k[2].offset);
  EXPECT_EQ(ACMatchClassification::DIM | ACMatchClassification::MATCH,
            spans_k[2].style);
  EXPECT_EQ(9U, spans_k[3].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_k[3].style);

  // Multiple matches with both beginning and end at beginning of
  // classifications merge properly.
  matches.clear();
  matches.push_back(ACMatchClassification(0, ACMatchClassification::URL));
  matches.push_back(ACMatchClassification(11, ACMatchClassification::NONE));
  ClassifyTest classify_test7(ASCIIToUTF16("http://a.co is great"), matches);

  ACMatchClassifications spans_l =
      classify_test7.RunTest(ASCIIToUTF16("ht co"));
  ASSERT_EQ(4U, spans_l.size());
  EXPECT_EQ(0U, spans_l[0].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_l[0].style);
  EXPECT_EQ(2U, spans_l[1].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_l[1].style);
  EXPECT_EQ(9U, spans_l[2].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_l[2].style);
  EXPECT_EQ(11U, spans_l[3].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_l[3].style);
}

TEST_F(ShortcutsProviderTest, CalculateScore) {
  ACMatchClassifications spans_content;
  spans_content.push_back(ACMatchClassification(0, ACMatchClassification::URL));
  spans_content.push_back(ACMatchClassification(
      4, ACMatchClassification::MATCH | ACMatchClassification::URL));
  spans_content.push_back(ACMatchClassification(8, ACMatchClassification::URL));
  ACMatchClassifications spans_description;
  spans_description.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  spans_description.push_back(
      ACMatchClassification(2, ACMatchClassification::MATCH));
  ShortcutsBackend::Shortcut shortcut(std::string(),
      ASCIIToUTF16("test"), GURL("http://www.test.com"),
      ASCIIToUTF16("www.test.com"), spans_content, ASCIIToUTF16("A test"),
      spans_description, base::Time::Now(), 1);

  // Maximal score.
  const int kMaxScore = ShortcutsProvider::CalculateScore(
      ASCIIToUTF16("test"), shortcut);

  // Score decreases as percent of the match is decreased.
  int score_three_quarters =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("tes"), shortcut);
  EXPECT_LT(score_three_quarters, kMaxScore);
  int score_one_half =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("te"), shortcut);
  EXPECT_LT(score_one_half, score_three_quarters);
  int score_one_quarter =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("t"), shortcut);
  EXPECT_LT(score_one_quarter, score_one_half);

  // Should decay with time - one week.
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(7);
  int score_week_old =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut);
  EXPECT_LT(score_week_old, kMaxScore);

  // Should decay more in two weeks.
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_two_weeks_old =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut);
  EXPECT_LT(score_two_weeks_old, score_week_old);

  // But not if it was activly clicked on. 2 hits slow decaying power.
  shortcut.number_of_hits = 2;
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_popular_two_weeks_old =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut);
  EXPECT_LT(score_two_weeks_old, score_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_popular_two_weeks_old, kMaxScore);

  // 3 hits slow decaying power even more.
  shortcut.number_of_hits = 3;
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_more_popular_two_weeks_old =
      ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut);
  EXPECT_LT(score_two_weeks_old, score_more_popular_two_weeks_old);
  EXPECT_LT(score_popular_two_weeks_old, score_more_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_more_popular_two_weeks_old, kMaxScore);
}

TEST_F(ShortcutsProviderTest, DeleteMatch) {
  TestShortcutInfo shortcuts_to_test_delete[3] = {
    { "BD85DBA2-8C29-49F9-84AE-48E1E90880F1",
      "http://www.deletetest.com/1.html", "delete",
      "http://www.deletetest.com/1.html", "0,2",
      "Erase this shortcut!", "0,0", 1, 1},
    { "BD85DBA2-8C29-49F9-84AE-48E1E90880F2",
      "http://www.deletetest.com/1.html", "erase",
      "http://www.deletetest.com/1.html", "0,2",
      "Erase this shortcut!", "0,0", 1, 1},
    { "BD85DBA2-8C29-49F9-84AE-48E1E90880F3",
      "http://www.deletetest.com/2.html", "delete",
      "http://www.deletetest.com/2.html", "0,2",
      "Erase this shortcut!", "0,0", 1, 1},
  };

  size_t original_shortcuts_count = backend_->shortcuts_map().size();

  FillData(shortcuts_to_test_delete, arraysize(shortcuts_to_test_delete));

  EXPECT_EQ(original_shortcuts_count + 3, backend_->shortcuts_map().size());
  EXPECT_FALSE(backend_->shortcuts_map().end() ==
               backend_->shortcuts_map().find(ASCIIToUTF16("delete")));
  EXPECT_FALSE(backend_->shortcuts_map().end() ==
               backend_->shortcuts_map().find(ASCIIToUTF16("erase")));

  AutocompleteMatch match(provider_, 1200, true,
                          AutocompleteMatch::HISTORY_TITLE);

  match.destination_url = GURL(shortcuts_to_test_delete[0].url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[0].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[0].description);

  provider_->DeleteMatch(match);

  // |shortcuts_to_test_delete[0]| and |shortcuts_to_test_delete[1]| should be
  // deleted, but not |shortcuts_to_test_delete[2]| as it has different url.
  EXPECT_EQ(original_shortcuts_count + 1, backend_->shortcuts_map().size());
  EXPECT_FALSE(backend_->shortcuts_map().end() ==
               backend_->shortcuts_map().find(ASCIIToUTF16("delete")));
  EXPECT_TRUE(backend_->shortcuts_map().end() ==
              backend_->shortcuts_map().find(ASCIIToUTF16("erase")));

  match.destination_url = GURL(shortcuts_to_test_delete[2].url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[2].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[2].description);

  provider_->DeleteMatch(match);
  EXPECT_EQ(original_shortcuts_count, backend_->shortcuts_map().size());
  EXPECT_TRUE(backend_->shortcuts_map().end() ==
              backend_->shortcuts_map().find(ASCIIToUTF16("delete")));
}
