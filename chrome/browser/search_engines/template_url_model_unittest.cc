// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/scoped_vector.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_test_util.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

#if defined(OS_LINUX)
// Timed out on Chromium Linux.  http://crbug.com/53607
#define MAYBE_Load DISABLED_Load
#else
#define MAYBE_Load Load
#endif

// Test the GenerateSearchURL on a thread or the main thread.
class TestGenerateSearchURL
    : public base::RefCountedThreadSafe<TestGenerateSearchURL> {
 public:
  explicit TestGenerateSearchURL(SearchTermsData* search_terms_data)
      : search_terms_data_(search_terms_data),
        passed_(false) {
  }

  // Run the test cases for GenerateSearchURL.
  void RunTest();

  // Did the test pass?
  bool passed() const { return passed_; }

 private:
  friend class base::RefCountedThreadSafe<TestGenerateSearchURL>;
  ~TestGenerateSearchURL() {}

  SearchTermsData* search_terms_data_;
  bool passed_;

  DISALLOW_COPY_AND_ASSIGN(TestGenerateSearchURL);
};

// Simple implementation of SearchTermsData.
class TestSearchTermsData : public SearchTermsData {
 public:
  explicit TestSearchTermsData(const char* google_base_url)
      : google_base_url_(google_base_url)  {
  }

  virtual std::string GoogleBaseURLValue() const {
    return google_base_url_;
  }

  virtual std::string GetApplicationLocale() const {
    return "yy";
  }

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // Returns the value for the Chrome Omnibox rlz.
  virtual string16 GetRlzParameterValue() const {
    return string16();
  }
#endif

 private:
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchTermsData);
};

// Create an URL that appears to have been prepopulated, but won't be in the
// current data. The caller owns the returned TemplateURL*.
static TemplateURL* CreatePreloadedTemplateURL() {
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL("http://www.unittest.com/", 0, 0);
  t_url->set_keyword(ASCIIToUTF16("unittest"));
  t_url->set_short_name(ASCIIToUTF16("unittest"));
  t_url->set_safe_for_autoreplace(true);
  GURL favicon_url("http://favicon.url");
  t_url->SetFavIconURL(favicon_url);
  t_url->set_date_created(Time::FromTimeT(100));
  t_url->set_prepopulate_id(999999);
  return t_url;
}

class TemplateURLModelTest : public testing::Test {
 public:
  TemplateURLModelTest() {}

  virtual void SetUp() {
    test_util_.SetUp();
  }

  virtual void TearDown() {
    test_util_.TearDown();
  }

  TemplateURL* AddKeywordWithDate(const std::string& keyword,
                                  bool autogenerate_keyword,
                                  const std::string& url,
                                  const std::string& suggest_url,
                                  const std::string& fav_icon_url,
                                  const std::string& encodings,
                                  const std::string& short_name,
                                  bool safe_for_autoreplace,
                                  Time created_date) {
    TemplateURL* template_url = new TemplateURL();
    template_url->SetURL(url, 0, 0);
    template_url->SetSuggestionsURL(suggest_url, 0, 0);
    template_url->SetFavIconURL(GURL(fav_icon_url));
    template_url->set_keyword(UTF8ToUTF16(keyword));
    template_url->set_autogenerate_keyword(autogenerate_keyword);
    template_url->set_short_name(UTF8ToUTF16(short_name));
    std::vector<std::string> encodings_vector;
    base::SplitString(encodings, ';', &encodings_vector);
    template_url->set_input_encodings(encodings_vector);
    template_url->set_date_created(created_date);
    template_url->set_safe_for_autoreplace(safe_for_autoreplace);
    model()->Add(template_url);
    EXPECT_NE(0, template_url->id());
    return template_url;
  }

  // Simulate firing by the prefs service specifying that the managed
  // preferences have changed.
  void NotifyManagedPrefsHaveChanged() {
    model()->Observe(
        NotificationType::PREF_CHANGED,
        Source<PrefService>(profile()->GetTestingPrefService()),
        Details<std::string>(NULL));
  }

  // Verifies the two TemplateURLs are equal.
  void AssertEquals(const TemplateURL& expected, const TemplateURL& actual) {
    ASSERT_TRUE(TemplateURLRef::SameUrlRefs(expected.url(), actual.url()));
    ASSERT_TRUE(TemplateURLRef::SameUrlRefs(expected.suggestions_url(),
                                            actual.suggestions_url()));
    ASSERT_EQ(expected.keyword(), actual.keyword());
    ASSERT_EQ(expected.short_name(), actual.short_name());
    ASSERT_EQ(JoinString(expected.input_encodings(), ';'),
              JoinString(actual.input_encodings(), ';'));
    ASSERT_TRUE(expected.GetFavIconURL() == actual.GetFavIconURL());
    ASSERT_EQ(expected.id(), actual.id());
    ASSERT_EQ(expected.safe_for_autoreplace(), actual.safe_for_autoreplace());
    ASSERT_EQ(expected.show_in_default_list(), actual.show_in_default_list());
    ASSERT_TRUE(expected.date_created() == actual.date_created());
  }

  // Checks that the two TemplateURLs are similar. It does not check the id
  // and the date_created.  Neither pointer should be NULL.
  void ExpectSimilar(const TemplateURL* expected, const TemplateURL* actual) {
    ASSERT_TRUE(expected != NULL);
    ASSERT_TRUE(actual != NULL);
    EXPECT_TRUE(TemplateURLRef::SameUrlRefs(expected->url(), actual->url()));
    EXPECT_TRUE(TemplateURLRef::SameUrlRefs(expected->suggestions_url(),
                                            actual->suggestions_url()));
    EXPECT_EQ(expected->keyword(), actual->keyword());
    EXPECT_EQ(expected->short_name(), actual->short_name());
    EXPECT_EQ(JoinString(expected->input_encodings(), ';'),
              JoinString(actual->input_encodings(), ';'));
    EXPECT_TRUE(expected->GetFavIconURL() == actual->GetFavIconURL());
    EXPECT_EQ(expected->safe_for_autoreplace(), actual->safe_for_autoreplace());
    EXPECT_EQ(expected->show_in_default_list(), actual->show_in_default_list());
  }

  // Set the managed preferences for the default search provider and trigger
  // notification.
  void SetManagedDefaultSearchPreferences(bool enabled,
                                          const char* name,
                                          const char* search_url,
                                          const char* suggest_url,
                                          const char* icon_url,
                                          const char* encodings,
                                          const char* keyword) {
    TestingPrefService* service = profile()->GetTestingPrefService();
    service->SetManagedPref(
        prefs::kDefaultSearchProviderEnabled,
        Value::CreateBooleanValue(enabled));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderName,
        Value::CreateStringValue(name));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderSearchURL,
        Value::CreateStringValue(search_url));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderSuggestURL,
        Value::CreateStringValue(suggest_url));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderIconURL,
        Value::CreateStringValue(icon_url));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderEncodings,
        Value::CreateStringValue(encodings));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderKeyword,
        Value::CreateStringValue(keyword));
  }

  // Remove all the managed preferences for the default search provider and
  // trigger notification.
  void RemoveManagedDefaultSearchPreferences() {
    TestingPrefService* service = profile()->GetTestingPrefService();
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderSearchURL);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderEnabled);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderName);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderSuggestURL);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderIconURL);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderEncodings);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderKeyword);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderID);
    service->RemoveManagedPref(
        prefs::kDefaultSearchProviderPrepopulateID);
  }

  // Creates a TemplateURL with the same prepopulated id as a real prepopulated
  // item. The input number determines which prepopulated item. The caller is
  // responsible for owning the returned TemplateURL*.
  TemplateURL* CreateReplaceablePreloadedTemplateURL(
      size_t index_offset_from_default,
      string16* prepopulated_display_url);

  // Verifies the behavior of when a preloaded url later gets changed.
  // Since the input is the offset from the default, when one passes in
  // 0, it tests the default. Passing in a number > 0 will verify what
  // happens when a preloaded url that is not the default gets updated.
  void TestLoadUpdatingPreloadedURL(size_t index_offset_from_default);

  // Helper methods to make calling TemplateURLModelTestUtil methods less
  // visually noisy in the test code.
  void VerifyObserverCount(int expected_changed_count) {
    EXPECT_EQ(expected_changed_count, test_util_.GetObserverCount());
    test_util_.ResetObserverCount();
  }
  void VerifyObserverFired() {
    EXPECT_LE(1, test_util_.GetObserverCount());
    test_util_.ResetObserverCount();
  }
  void BlockTillServiceProcessesRequests() {
    TemplateURLModelTestUtil::BlockTillServiceProcessesRequests();
  }
  void VerifyLoad() { test_util_.VerifyLoad(); }
  void ChangeModelToLoadState() { test_util_.ChangeModelToLoadState(); }
  void ResetModel(bool verify_load) { test_util_.ResetModel(verify_load); }
  string16 GetAndClearSearchTerm() {
    return test_util_.GetAndClearSearchTerm();
  }
  void SetGoogleBaseURL(const std::string& base_url) const {
    test_util_.SetGoogleBaseURL(base_url);
  }
  WebDataService* GetWebDataService() { return test_util_.GetWebDataService(); }
  TemplateURLModel* model() { return test_util_.model(); }
  TestingProfile* profile() { return test_util_.profile(); }

 protected:
  TemplateURLModelTestUtil test_util_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLModelTest);
};

void TestGenerateSearchURL::RunTest() {
  struct GenerateSearchURLCase {
    const char* test_name;
    const char* url;
    const char* expected;
  } generate_url_cases[] = {
    { "empty TemplateURLRef", NULL, "" },
    { "invalid URL", "foo{searchTerms}", "" },
    { "URL with no replacements", "http://foo/", "http://foo/" },
    { "basic functionality", "http://foo/{searchTerms}",
      "http://foo/blah.blah.blah.blah.blah" }
  };

  // Don't use ASSERT/EXPECT since this is run on a thread in one test
  // and those macros aren't meant for threads at this time according to
  // gtest documentation.
  bool everything_passed = true;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(generate_url_cases); ++i) {
    TemplateURL t_url;
    if (generate_url_cases[i].url)
      t_url.SetURL(generate_url_cases[i].url, 0, 0);

    std::string result = search_terms_data_ ?
        TemplateURLModel::GenerateSearchURLUsingTermsData(
            &t_url, *search_terms_data_).spec() :
        TemplateURLModel::GenerateSearchURL(&t_url).spec();
    if (strcmp(generate_url_cases[i].expected, result.c_str())) {
      LOG(ERROR) << generate_url_cases[i].test_name << " failed. Expected " <<
          generate_url_cases[i].expected << " Actual " << result;

      everything_passed = false;
    }
  }
  passed_ = everything_passed;
}

TemplateURL* TemplateURLModelTest::CreateReplaceablePreloadedTemplateURL(
    size_t index_offset_from_default,
    string16* prepopulated_display_url) {
  TemplateURL* t_url = CreatePreloadedTemplateURL();
  ScopedVector<TemplateURL> prepopulated_urls;
  size_t default_search_provider_index = 0;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(
      profile()->GetPrefs(),
      &prepopulated_urls.get(),
      &default_search_provider_index);
  EXPECT_LT(index_offset_from_default, prepopulated_urls.size());
  size_t prepopulated_index =
      (default_search_provider_index + index_offset_from_default) %
      prepopulated_urls.size();
  t_url->set_prepopulate_id(
      prepopulated_urls[prepopulated_index]->prepopulate_id());
  *prepopulated_display_url =
      prepopulated_urls[prepopulated_index]->url()->DisplayURL();
  return t_url;
}

void TemplateURLModelTest::TestLoadUpdatingPreloadedURL(
    size_t index_offset_from_default) {
  string16 prepopulated_url;
  TemplateURL* t_url = CreateReplaceablePreloadedTemplateURL(
      index_offset_from_default, &prepopulated_url);
  t_url->set_safe_for_autoreplace(false);

  string16 original_url = t_url->url()->DisplayURL();
  ASSERT_NE(prepopulated_url, original_url);

  // Then add it to the model and save it all.
  ChangeModelToLoadState();
  model()->Add(t_url);
  const TemplateURL* keyword_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_EQ(t_url, keyword_url);
  ASSERT_EQ(original_url, keyword_url->url()->DisplayURL());
  BlockTillServiceProcessesRequests();

  // Now reload the model and verify that the merge updates the url.
  ResetModel(true);
  keyword_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(keyword_url != NULL);
  ASSERT_EQ(prepopulated_url, keyword_url->url()->DisplayURL());

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();

  // Reload the model to verify that change was saved correctly.
  ResetModel(true);
  keyword_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(keyword_url != NULL);
  ASSERT_EQ(prepopulated_url, keyword_url->url()->DisplayURL());
}

TEST_F(TemplateURLModelTest, MAYBE_Load) {
  VerifyLoad();
}

TEST_F(TemplateURLModelTest, AddUpdateRemove) {
  // Add a new TemplateURL.
  VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL("http://www.google.com/foo/bar", 0, 0);
  t_url->set_keyword(ASCIIToUTF16("keyword"));
  t_url->set_short_name(ASCIIToUTF16("google"));
  GURL favicon_url("http://favicon.url");
  t_url->SetFavIconURL(favicon_url);
  t_url->set_date_created(Time::FromTimeT(100));
  t_url->set_safe_for_autoreplace(true);
  model()->Add(t_url);
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("keyword"),
                                         GURL(), NULL));
  VerifyObserverCount(1);
  BlockTillServiceProcessesRequests();
  // We need to clone as model takes ownership of TemplateURL and will
  // delete it.
  TemplateURL cloned_url(*t_url);
  ASSERT_EQ(1 + initial_count, model()->GetTemplateURLs().size());
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(t_url->keyword()) == t_url);
  ASSERT_TRUE(t_url->date_created() == cloned_url.date_created());

  // Reload the model to verify it was actually saved to the database.
  ResetModel(true);
  ASSERT_EQ(1 + initial_count, model()->GetTemplateURLs().size());
  const TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(cloned_url, *loaded_url);
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("keyword"),
                                         GURL(), NULL));

  // Mutate an element and verify it succeeded.
  model()->ResetTemplateURL(loaded_url, ASCIIToUTF16("a"),
                            ASCIIToUTF16("b"), "c");
  ASSERT_EQ(ASCIIToUTF16("a"), loaded_url->short_name());
  ASSERT_EQ(ASCIIToUTF16("b"), loaded_url->keyword());
  ASSERT_EQ("c", loaded_url->url()->url());
  ASSERT_FALSE(loaded_url->safe_for_autoreplace());
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("keyword"),
                                         GURL(), NULL));
  ASSERT_FALSE(model()->CanReplaceKeyword(ASCIIToUTF16("b"), GURL(), NULL));
  cloned_url = *loaded_url;
  BlockTillServiceProcessesRequests();
  ResetModel(true);
  ASSERT_EQ(1 + initial_count, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("b"));
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(cloned_url, *loaded_url);

  // Remove an element and verify it succeeded.
  model()->Remove(loaded_url);
  VerifyObserverCount(1);
  ResetModel(true);
  ASSERT_EQ(initial_count, model()->GetTemplateURLs().size());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("b")) == NULL);
}

TEST_F(TemplateURLModelTest, GenerateKeyword) {
  ASSERT_EQ(string16(), TemplateURLModel::GenerateKeyword(GURL(), true));
  // Shouldn't generate keywords for https.
  ASSERT_EQ(string16(),
            TemplateURLModel::GenerateKeyword(GURL("https://blah"), true));
  ASSERT_EQ(ASCIIToUTF16("foo"),
            TemplateURLModel::GenerateKeyword(GURL("http://foo"), true));
  // www. should be stripped.
  ASSERT_EQ(ASCIIToUTF16("foo"),
            TemplateURLModel::GenerateKeyword(GURL("http://www.foo"), true));
  // Shouldn't generate keywords with paths, if autodetected.
  ASSERT_EQ(string16(),
            TemplateURLModel::GenerateKeyword(GURL("http://blah/foo"), true));
  ASSERT_EQ(ASCIIToUTF16("blah"),
            TemplateURLModel::GenerateKeyword(GURL("http://blah/foo"), false));
  // FTP shouldn't generate a keyword.
  ASSERT_EQ(string16(),
            TemplateURLModel::GenerateKeyword(GURL("ftp://blah/"), true));
  // Make sure we don't get a trailing /
  ASSERT_EQ(ASCIIToUTF16("blah"),
            TemplateURLModel::GenerateKeyword(GURL("http://blah/"), true));
}

TEST_F(TemplateURLModelTest, GenerateSearchURL) {
  scoped_refptr<TestGenerateSearchURL> test_generate_search_url(
      new TestGenerateSearchURL(NULL));
  test_generate_search_url->RunTest();
  EXPECT_TRUE(test_generate_search_url->passed());
}

TEST_F(TemplateURLModelTest, GenerateSearchURLUsingTermsData) {
  // Run the test for GenerateSearchURLUsingTermsData on the "IO" thread and
  // wait for it to finish.
  TestSearchTermsData search_terms_data("http://google.com/");
  scoped_refptr<TestGenerateSearchURL> test_generate_search_url(
      new TestGenerateSearchURL(&search_terms_data));

  test_util_.StartIOThread();
  BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)->PostTask(
          FROM_HERE,
          NewRunnableMethod(test_generate_search_url.get(),
                            &TestGenerateSearchURL::RunTest));
  TemplateURLModelTestUtil::BlockTillIOThreadProcessesRequests();
  EXPECT_TRUE(test_generate_search_url->passed());
}

TEST_F(TemplateURLModelTest, ClearBrowsingData_Keywords) {
  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

  // Nothing has been added.
  EXPECT_EQ(0U, model()->GetTemplateURLs().size());

  // Create one with a 0 time.
  AddKeywordWithDate("key1", false, "http://foo1", "http://suggest1",
                     "http://icon1", "UTF-8;UTF-16", "name1", true, Time());
  // Create one for now and +/- 1 day.
  AddKeywordWithDate("key2", false, "http://foo2", "http://suggest2",
                     "http://icon2", "UTF-8;UTF-16", "name2", true,
                     now - one_day);
  AddKeywordWithDate("key3", false, "http://foo3", "", "", "", "name3",
                     true, now);
  AddKeywordWithDate("key4", false, "http://foo4", "", "", "", "name4",
                     true, now + one_day);
  // Try the other three states.
  AddKeywordWithDate("key5", false, "http://foo5", "http://suggest5",
                     "http://icon5", "UTF-8;UTF-16", "name5", false, now);
  AddKeywordWithDate("key6", false, "http://foo6", "http://suggest6",
                     "http://icon6", "UTF-8;UTF-16", "name6", false,
                     month_ago);

  // We just added a few items, validate them.
  EXPECT_EQ(6U, model()->GetTemplateURLs().size());

  // Try removing from current timestamp. This should delete the one in the
  // future and one very recent one.
  model()->RemoveAutoGeneratedSince(now);
  EXPECT_EQ(4U, model()->GetTemplateURLs().size());

  // Try removing from two months ago. This should only delete items that are
  // auto-generated.
  model()->RemoveAutoGeneratedSince(now - TimeDelta::FromDays(60));
  EXPECT_EQ(3U, model()->GetTemplateURLs().size());

  // Make sure the right values remain.
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(0U,
            model()->GetTemplateURLs()[0]->date_created().ToInternalValue());

  EXPECT_EQ(ASCIIToUTF16("key5"), model()->GetTemplateURLs()[1]->keyword());
  EXPECT_FALSE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());
  EXPECT_EQ(now.ToInternalValue(),
            model()->GetTemplateURLs()[1]->date_created().ToInternalValue());

  EXPECT_EQ(ASCIIToUTF16("key6"), model()->GetTemplateURLs()[2]->keyword());
  EXPECT_FALSE(model()->GetTemplateURLs()[2]->safe_for_autoreplace());
  EXPECT_EQ(month_ago.ToInternalValue(),
            model()->GetTemplateURLs()[2]->date_created().ToInternalValue());

  // Try removing from Time=0. This should delete one more.
  model()->RemoveAutoGeneratedSince(Time());
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
}

TEST_F(TemplateURLModelTest, Reset) {
  // Add a new TemplateURL.
  VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL("http://www.google.com/foo/bar", 0, 0);
  t_url->set_keyword(ASCIIToUTF16("keyword"));
  t_url->set_short_name(ASCIIToUTF16("google"));
  GURL favicon_url("http://favicon.url");
  t_url->SetFavIconURL(favicon_url);
  t_url->set_date_created(Time::FromTimeT(100));
  model()->Add(t_url);

  VerifyObserverCount(1);
  BlockTillServiceProcessesRequests();

  // Reset the short name, keyword, url and make sure it takes.
  const string16 new_short_name(ASCIIToUTF16("a"));
  const string16 new_keyword(ASCIIToUTF16("b"));
  const std::string new_url("c");
  model()->ResetTemplateURL(t_url, new_short_name, new_keyword, new_url);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(new_keyword, t_url->keyword());
  ASSERT_EQ(new_url, t_url->url()->url());

  // Make sure the mappings in the model were updated.
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(new_keyword) == t_url);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")) == NULL);

  TemplateURL last_url = *t_url;

  // Reload the model from the database and make sure the change took.
  ResetModel(true);
  t_url = NULL;
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* read_url = model()->GetTemplateURLForKeyword(new_keyword);
  ASSERT_TRUE(read_url);
  AssertEquals(last_url, *read_url);
}

TEST_F(TemplateURLModelTest, DefaultSearchProvider) {
  // Add a new TemplateURL.
  VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURL* t_url = AddKeywordWithDate("key1", false, "http://foo1",
      "http://sugg1", "http://icon1", "UTF-8;UTF-16", "name1", true, Time());

  test_util_.ResetObserverCount();
  model()->SetDefaultSearchProvider(t_url);

  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());

  ASSERT_TRUE(t_url->safe_for_autoreplace());
  ASSERT_TRUE(t_url->show_in_default_list());

  // Setting the default search provider should have caused notification.
  VerifyObserverCount(1);

  BlockTillServiceProcessesRequests();

  TemplateURL cloned_url = *t_url;

  ResetModel(true);
  t_url = NULL;

  // Make sure when we reload we get a default search provider.
  EXPECT_EQ(1 + initial_count, model()->GetTemplateURLs().size());
  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(cloned_url, *model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLModelTest, TemplateURLWithNoKeyword) {
  VerifyLoad();

  const size_t initial_count = model()->GetTemplateURLs().size();

  AddKeywordWithDate("", false, "http://foo1", "http://sugg1",
      "http://icon1", "UTF-8;UTF-16", "name1", true, Time());

  // We just added a few items, validate them.
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Reload the model from the database and make sure we get the url back.
  ResetModel(true);

  ASSERT_EQ(1 + initial_count, model()->GetTemplateURLs().size());

  bool found_keyword = false;
  for (size_t i = 0; i < initial_count + 1; ++i) {
    if (model()->GetTemplateURLs()[i]->keyword().empty()) {
      found_keyword = true;
      break;
    }
  }
  ASSERT_TRUE(found_keyword);
}

TEST_F(TemplateURLModelTest, CantReplaceWithSameKeyword) {
  ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"), GURL(), NULL));
  TemplateURL* t_url = AddKeywordWithDate("foo", false, "http://foo1",
      "http://sugg1", "http://icon1", "UTF-8;UTF-16",  "name1", true, Time());

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"),
                                         GURL("http://foo2"), NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                           t_url->url()->url());

  ASSERT_FALSE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"),
                                          GURL("http://foo2"), NULL));
}

TEST_F(TemplateURLModelTest, CantReplaceWithSameHosts) {
  ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"),
                                         GURL("http://foo.com"), NULL));
  TemplateURL* t_url = AddKeywordWithDate("foo", false, "http://foo.com",
      "http://sugg1", "http://icon1", "UTF-8;UTF-16",  "name1", true, Time());

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("bar"),
                                         GURL("http://foo.com"), NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                           t_url->url()->url());

  ASSERT_FALSE(model()->CanReplaceKeyword(ASCIIToUTF16("bar"),
                                          GURL("http://foo.com"), NULL));
}

TEST_F(TemplateURLModelTest, HasDefaultSearchProvider) {
  // We should have a default search provider even if we haven't loaded.
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Now force the model to load and make sure we still have a default.
  VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLModelTest, DefaultSearchProviderLoadedFromPrefs) {
  VerifyLoad();

  TemplateURL* template_url = new TemplateURL();
  template_url->SetURL("http://url", 0, 0);
  template_url->SetSuggestionsURL("http://url2", 0, 0);
  template_url->SetInstantURL("http://instant", 0, 0);
  template_url->set_short_name(ASCIIToUTF16("a"));
  template_url->set_safe_for_autoreplace(true);
  template_url->set_date_created(Time::FromTimeT(100));

  model()->Add(template_url);

  const TemplateURLID id = template_url->id();

  model()->SetDefaultSearchProvider(template_url);

  BlockTillServiceProcessesRequests();

  TemplateURL first_default_search_provider = *template_url;

  template_url = NULL;

  // Reset the model and don't load it. The template url we set as the default
  // should be pulled from prefs now.
  ResetModel(false);

  // NOTE: This doesn't use AssertEquals as only a subset of the TemplateURLs
  // value are persisted to prefs.
  const TemplateURL* default_turl = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_turl);
  ASSERT_TRUE(default_turl->url());
  ASSERT_EQ("http://url", default_turl->url()->url());
  ASSERT_TRUE(default_turl->suggestions_url());
  ASSERT_EQ("http://url2", default_turl->suggestions_url()->url());
  ASSERT_TRUE(default_turl->instant_url());
  EXPECT_EQ("http://instant", default_turl->instant_url()->url());
  ASSERT_EQ(ASCIIToUTF16("a"), default_turl->short_name());
  ASSERT_EQ(id, default_turl->id());

  // Now do a load and make sure the default search provider really takes.
  VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(first_default_search_provider,
               *model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLModelTest, BuildQueryTerms) {
  struct TestData {
    const std::string url;
    const bool result;
    // Keys and values are a semicolon separated list of expected values in the
    // map.
    const std::string keys;
    const std::string values;
  } data[] = {
    // No query should return false.
    { "http://blah/", false, "", "" },

    // Query with empty key should return false.
    { "http://blah/foo?=y", false, "", "" },

    // Query with key occurring multiple times should return false.
    { "http://blah/foo?x=y&x=z", false, "", "" },

    { "http://blah/foo?x=y", true, "x", "y" },
    { "http://blah/foo?x=y&y=z", true, "x;y", "y;z" },

    // Key occurring multiple times should get an empty string.
    { "http://blah/foo?x=y&x=z&y=z", true, "x;y", ";z" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    TemplateURLModel::QueryTerms terms;
    ASSERT_EQ(data[i].result,
              TemplateURLModel::BuildQueryTerms(GURL(data[i].url), &terms));
    if (data[i].result) {
      std::vector<std::string> keys;
      std::vector<std::string> values;
      base::SplitString(data[i].keys, ';', &keys);
      base::SplitString(data[i].values, ';', &values);
      ASSERT_TRUE(keys.size() == values.size());
      ASSERT_EQ(keys.size(), terms.size());
      for (size_t j = 0; j < keys.size(); ++j) {
        TemplateURLModel::QueryTerms::iterator term_iterator =
            terms.find(keys[j]);
        ASSERT_TRUE(term_iterator != terms.end());
        ASSERT_EQ(values[j], term_iterator->second);
      }
    }
  }
}

TEST_F(TemplateURLModelTest, UpdateKeywordSearchTermsForURL) {
  struct TestData {
    const std::string url;
    const string16 term;
  } data[] = {
    { "http://foo/", string16() },
    { "http://foo/foo?q=xx", string16() },
    { "http://x/bar?q=xx", string16() },
    { "http://x/foo?y=xx", string16() },
    { "http://x/foo?q=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?a=b&q=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?q=b&q=xx", string16() },
  };

  ChangeModelToLoadState();
  AddKeywordWithDate("x", false, "http://x/foo?q={searchTerms}",
      "http://sugg1", "http://icon1", "UTF-8;UTF-16", "name", false, Time());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLVisitedDetails details;
    details.row = history::URLRow(GURL(data[i].url));
    details.transition = 0;
    model()->UpdateKeywordSearchTermsForURL(details);
    EXPECT_EQ(data[i].term, GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLModelTest, DontUpdateKeywordSearchForNonReplaceable) {
  struct TestData {
    const std::string url;
  } data[] = {
    { "http://foo/" },
    { "http://x/bar?q=xx" },
    { "http://x/foo?y=xx" },
  };

  ChangeModelToLoadState();
  AddKeywordWithDate("x", false, "http://x/foo", "http://sugg1",
      "http://icon1", "UTF-8;UTF-16", "name", false, Time());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLVisitedDetails details;
    details.row = history::URLRow(GURL(data[i].url));
    details.transition = 0;
    model()->UpdateKeywordSearchTermsForURL(details);
    ASSERT_EQ(string16(), GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLModelTest, ChangeGoogleBaseValue) {
  // NOTE: Do not do a VerifyLoad() here as it will load the prepopulate data,
  // which also has a {google:baseURL} keyword in it, which will confuse this
  // test.
  ChangeModelToLoadState();
  SetGoogleBaseURL("http://google.com/");
  const TemplateURL* t_url = AddKeywordWithDate("", true,
      "{google:baseURL}?q={searchTerms}", "http://sugg1", "http://icon1",
      "UTF-8;UTF-16", "name", false, Time());
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("google.com"));
  EXPECT_EQ("google.com", t_url->url()->GetHost());
  EXPECT_EQ(ASCIIToUTF16("google.com"), t_url->keyword());

  // Change the Google base url.
  test_util_.ResetObserverCount();
  SetGoogleBaseURL("http://foo.com/");
  VerifyObserverCount(1);

  // Make sure the host->TemplateURL map was updated appropriately.
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("foo.com"));
  EXPECT_TRUE(model()->GetTemplateURLForHost("google.com") == NULL);
  EXPECT_EQ("foo.com", t_url->url()->GetHost());
  EXPECT_EQ(ASCIIToUTF16("foo.com"), t_url->keyword());
  EXPECT_EQ("http://foo.com/?q=x", t_url->url()->ReplaceSearchTerms(*t_url,
      ASCIIToUTF16("x"), TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
}

struct QueryHistoryCallbackImpl {
  QueryHistoryCallbackImpl() : success(false) {}

  void Callback(HistoryService::Handle handle,
                bool success, const history::URLRow* row,
                history::VisitVector* visits) {
    this->success = success;
    if (row)
      this->row = *row;
    if (visits)
      this->visits = *visits;
  }

  bool success;
  history::URLRow row;
  history::VisitVector visits;
};

// Make sure TemplateURLModel generates a KEYWORD_GENERATED visit for
// KEYWORD visits.
TEST_F(TemplateURLModelTest, GenerateVisitOnKeyword) {
  VerifyLoad();
  profile()->CreateHistoryService(true, false);

  // Create a keyword.
  TemplateURL* t_url = AddKeywordWithDate(
      "keyword", false, "http://foo.com/foo?query={searchTerms}",
      "http://sugg1", "http://icon1", "UTF-8;UTF-16", "keyword",
      true, base::Time::Now());

  // Add a visit that matches the url of the keyword.
  HistoryService* history =
      profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history->AddPage(
      GURL(t_url->url()->ReplaceSearchTerms(*t_url, ASCIIToUTF16("blah"), 0,
                                            string16())),
      NULL, 0, GURL(), PageTransition::KEYWORD, history::RedirectList(),
      history::SOURCE_BROWSED, false);

  // Wait for history to finish processing the request.
  profile()->BlockUntilHistoryProcessesPendingRequests();

  // Query history for the generated url.
  CancelableRequestConsumer consumer;
  QueryHistoryCallbackImpl callback;
  history->QueryURL(GURL("http://keyword"), true, &consumer,
      NewCallback(&callback, &QueryHistoryCallbackImpl::Callback));

  // Wait for the request to be processed.
  profile()->BlockUntilHistoryProcessesPendingRequests();

  // And make sure the url and visit were added.
  EXPECT_TRUE(callback.success);
  EXPECT_NE(0, callback.row.id());
  ASSERT_EQ(1U, callback.visits.size());
  EXPECT_EQ(PageTransition::KEYWORD_GENERATED,
            PageTransition::StripQualifier(callback.visits[0].transition));
}

// Make sure that the load routine deletes prepopulated engines that no longer
// exist in the prepopulate data.
TEST_F(TemplateURLModelTest, LoadDeletesUnusedProvider) {
  // Create a preloaded template url. Add it to a loaded model and wait for the
  // saves to finish.
  TemplateURL* t_url = CreatePreloadedTemplateURL();
  ChangeModelToLoadState();
  model()->Add(t_url);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) != NULL);
  BlockTillServiceProcessesRequests();

  // Ensure that merging clears this engine.
  ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) == NULL);

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();

  // Reload the model to verify that the database was updated as a result of the
  // merge.
  ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) == NULL);
}

// Make sure that load routine doesn't delete prepopulated engines that no
// longer exist in the prepopulate data if it has been modified by the user.
TEST_F(TemplateURLModelTest, LoadRetainsModifiedProvider) {
  // Create a preloaded template url and add it to a loaded model.
  TemplateURL* t_url = CreatePreloadedTemplateURL();
  t_url->set_safe_for_autoreplace(false);
  ChangeModelToLoadState();
  model()->Add(t_url);

  // Do the copy after t_url is added so that the id is set.
  TemplateURL copy_t_url = *t_url;
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")));

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();

  // Ensure that merging won't clear it if the user has edited it.
  ResetModel(true);
  const TemplateURL* url_for_unittest =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(url_for_unittest != NULL);
  AssertEquals(copy_t_url, *url_for_unittest);

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();

  // Reload the model to verify that save/reload retains the item.
  ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) != NULL);
}

// Make sure that load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it has been modified by the user.
TEST_F(TemplateURLModelTest, LoadSavesPrepopulatedDefaultSearchProvider) {
  VerifyLoad();
  // Verify that the default search provider is set to something.
  ASSERT_TRUE(model()->GetDefaultSearchProvider() != NULL);
  TemplateURL default_url = *model()->GetDefaultSearchProvider();

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();

  // Reload the model and check that the default search provider
  // was properly saved.
  ResetModel(true);
  ASSERT_TRUE(model()->GetDefaultSearchProvider() != NULL);
  AssertEquals(default_url, *model()->GetDefaultSearchProvider());
}

// Make sure that the load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it is the default search provider.
TEST_F(TemplateURLModelTest, LoadRetainsDefaultProvider) {
  // Set the default search provider to a preloaded template url which
  // is not in the current set of preloaded template urls and save
  // the result.
  TemplateURL* t_url = CreatePreloadedTemplateURL();
  ChangeModelToLoadState();
  model()->Add(t_url);
  model()->SetDefaultSearchProvider(t_url);
  // Do the copy after t_url is added and set as default so that its
  // internal state is correct.
  TemplateURL copy_t_url = *t_url;

  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")));
  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());
  BlockTillServiceProcessesRequests();

  // Ensure that merging won't clear the prepopulated template url
  // which is no longer present if it's the default engine.
  ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
    ASSERT_TRUE(keyword_url != NULL);
    AssertEquals(copy_t_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();

  // Reload the model to verify that the update was saved.
  ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
    ASSERT_TRUE(keyword_url != NULL);
    AssertEquals(copy_t_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }
}

// Make sure that the load routine updates the url of a preexisting
// default search engine provider and that the result is saved correctly.
TEST_F(TemplateURLModelTest, LoadUpdatesDefaultSearchURL) {
  TestLoadUpdatingPreloadedURL(0);
}

// Make sure that the load routine updates the url of a preexisting
// non-default search engine provider and that the result is saved correctly.
TEST_F(TemplateURLModelTest, LoadUpdatesSearchURL) {
  TestLoadUpdatingPreloadedURL(1);
}

// Make sure that the load does update of auto-keywords correctly.
// This test basically verifies that no asserts or crashes occur
// during this operation.
TEST_F(TemplateURLModelTest, LoadDoesAutoKeywordUpdate) {
  string16 prepopulated_url;
  TemplateURL* t_url = CreateReplaceablePreloadedTemplateURL(
      0, &prepopulated_url);
  t_url->set_safe_for_autoreplace(false);
  t_url->SetURL("{google:baseURL}?q={searchTerms}", 0, 0);
  t_url->set_autogenerate_keyword(true);

  // Then add it to the model and save it all.
  ChangeModelToLoadState();
  model()->Add(t_url);
  BlockTillServiceProcessesRequests();

  // Now reload the model and verify that the merge updates the url.
  ResetModel(true);

  // Wait for any saves to finish.
  BlockTillServiceProcessesRequests();
}

// Simulates failing to load the webdb and makes sure the default search
// provider is valid.
TEST_F(TemplateURLModelTest, FailedInit) {
  VerifyLoad();

  test_util_.ClearModel();
  test_util_.GetWebDataService()->UnloadDatabase();
  test_util_.GetWebDataService()->set_failed_init(true);

  ResetModel(false);
  model()->Load();
  BlockTillServiceProcessesRequests();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

// Verifies that if the default search URL preference is managed, we report
// the default search as managed.  Also check that we are getting the right
// values.
TEST_F(TemplateURLModelTest, TestManagedDefaultSearch) {
  VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  test_util_.ResetObserverCount();

  // Set a regular default search provider.
  TemplateURL* regular_default = AddKeywordWithDate("key1", false,
      "http://foo1", "http://sugg1", "http://icon1", "UTF-8;UTF-16", "name1",
      true, Time());
  VerifyObserverCount(1);
  model()->SetDefaultSearchProvider(regular_default);
  // Adding the URL and setting the default search provider should have caused
  // notifications.
  VerifyObserverCount(1);
  EXPECT_FALSE(model()->is_default_search_managed());
  EXPECT_EQ(1 + initial_count, model()->GetTemplateURLs().size());

  // Set a managed preference that establishes a default search provider.
  const char kName[] = "test1";
  const char kSearchURL[] = "http://test.com/search?t={searchTerms}";
  const char kIconURL[] = "http://test.com/icon.jpg";
  const char kEncodings[] = "UTF-16;UTF-32";
  SetManagedDefaultSearchPreferences(true, kName, kSearchURL, "", kIconURL,
                                     kEncodings, "");
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(2 + initial_count, model()->GetTemplateURLs().size());

  // Verify that the default manager we are getting is the managed one.
  scoped_ptr<TemplateURL> expected_managed_default1(new TemplateURL());
  expected_managed_default1->SetURL(kSearchURL, 0, 0);
  expected_managed_default1->SetFavIconURL(GURL(kIconURL));
  expected_managed_default1->set_short_name(ASCIIToUTF16("test1"));
  std::vector<std::string> encodings_vector;
  base::SplitString(kEncodings, ';', &encodings_vector);
  expected_managed_default1->set_input_encodings(encodings_vector);
  expected_managed_default1->set_show_in_default_list(true);
  const TemplateURL* actual_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(actual_managed_default, expected_managed_default1.get());
  EXPECT_EQ(actual_managed_default->show_in_default_list(), true);

  // Update the managed preference and check that the model has changed.
  const char kNewName[] = "test2";
  const char kNewSearchURL[] = "http://other.com/search?t={searchTerms}";
  const char kNewSuggestURL[] = "http://other.com/suggest?t={searchTerms}";
  SetManagedDefaultSearchPreferences(true, kNewName, kNewSearchURL,
                                     kNewSuggestURL, "", "", "");
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(2 + initial_count, model()->GetTemplateURLs().size());

  // Verify that the default manager we are now getting is the correct one.
  scoped_ptr<TemplateURL> expected_managed_default2(new TemplateURL());
  expected_managed_default2->SetURL(kNewSearchURL, 0, 0);
  expected_managed_default2->SetSuggestionsURL(kNewSuggestURL, 0, 0);
  expected_managed_default2->set_short_name(ASCIIToUTF16("test2"));
  expected_managed_default2->set_show_in_default_list(true);
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(actual_managed_default, expected_managed_default2.get());
  EXPECT_EQ(actual_managed_default->show_in_default_list(), true);

  // Remove all the managed prefs and check that we are no longer managed.
  RemoveManagedDefaultSearchPreferences();
  VerifyObserverFired();
  EXPECT_FALSE(model()->is_default_search_managed());
  EXPECT_EQ(1 + initial_count, model()->GetTemplateURLs().size());

  // The default should now be the first URL added
  const TemplateURL* actual_final_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(actual_final_managed_default,
      model()->GetTemplateURLs()[0]);
  EXPECT_EQ(actual_final_managed_default->show_in_default_list(), true);

  // Disable the default search provider through policy.
  SetManagedDefaultSearchPreferences(false, "", "", "", "", "", "");
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_TRUE(NULL == model()->GetDefaultSearchProvider());
  EXPECT_EQ(1 + initial_count, model()->GetTemplateURLs().size());

  // Re-enable it.
  SetManagedDefaultSearchPreferences(true, kName, kSearchURL, "", kIconURL,
                                     kEncodings, "");
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(2 + initial_count, model()->GetTemplateURLs().size());

  // Verify that the default manager we are getting is the managed one.
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(actual_managed_default, expected_managed_default1.get());
  EXPECT_EQ(actual_managed_default->show_in_default_list(), true);
}
