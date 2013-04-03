// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/test/mock_time_provider.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webdata/common/web_database.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

// TestGenerateSearchURL ------------------------------------------------------

// Test the GenerateSearchURL on a thread or the main thread.
class TestGenerateSearchURL
    : public base::RefCountedThreadSafe<TestGenerateSearchURL> {
 public:
  explicit TestGenerateSearchURL(SearchTermsData* search_terms_data);

  // Run the test cases for GenerateSearchURL.
  void RunTest();

  // Did the test pass?
  bool passed() const { return passed_; }

 private:
  friend class base::RefCountedThreadSafe<TestGenerateSearchURL>;
  ~TestGenerateSearchURL();

  SearchTermsData* search_terms_data_;
  bool passed_;

  DISALLOW_COPY_AND_ASSIGN(TestGenerateSearchURL);
};

TestGenerateSearchURL::TestGenerateSearchURL(SearchTermsData* search_terms_data)
    : search_terms_data_(search_terms_data),
      passed_(false) {
}

void TestGenerateSearchURL::RunTest() {
  struct GenerateSearchURLCase {
    const char* test_name;
    const char* url;
    const char* expected;
  } generate_url_cases[] = {
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
    TemplateURLData data;
    data.SetURL(generate_url_cases[i].url);
    TemplateURL t_url(NULL, data);
    std::string result = (search_terms_data_ ?
        TemplateURLService::GenerateSearchURLUsingTermsData(&t_url,
            *search_terms_data_) :
        TemplateURLService::GenerateSearchURL(&t_url)).spec();
    if (result != generate_url_cases[i].expected) {
      LOG(ERROR) << generate_url_cases[i].test_name << " failed. Expected " <<
          generate_url_cases[i].expected << " Actual " << result;
      everything_passed = false;
    }
  }
  passed_ = everything_passed;
}

TestGenerateSearchURL::~TestGenerateSearchURL() {
}


// TestSearchTermsData --------------------------------------------------------

// Simple implementation of SearchTermsData.
class TestSearchTermsData : public SearchTermsData {
 public:
  explicit TestSearchTermsData(const char* google_base_url);

  virtual std::string GoogleBaseURLValue() const OVERRIDE;

 private:
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchTermsData);
};

TestSearchTermsData::TestSearchTermsData(const char* google_base_url)
    : google_base_url_(google_base_url)  {
}

std::string TestSearchTermsData::GoogleBaseURLValue() const {
  return google_base_url_;
}


// QueryHistoryCallbackImpl ---------------------------------------------------

struct QueryHistoryCallbackImpl {
  QueryHistoryCallbackImpl() : success(false) {}

  void Callback(HistoryService::Handle handle,
                bool success,
                const history::URLRow* row,
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

};  // namespace


// TemplateURLServiceTest -----------------------------------------------------

class TemplateURLServiceTest : public testing::Test {
 public:
  TemplateURLServiceTest();

  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  TemplateURL* AddKeywordWithDate(const std::string& short_name,
                                  const std::string& keyword,
                                  const std::string& url,
                                  const std::string& suggest_url,
                                  const std::string& alternate_url,
                                  const std::string& favicon_url,
                                  bool safe_for_autoreplace,
                                  const std::string& encodings,
                                  Time date_created,
                                  Time last_modified);

  // Verifies the two TemplateURLs are equal.
  void AssertEquals(const TemplateURL& expected, const TemplateURL& actual);

  // Checks that the two TemplateURLs are similar. It does not check the id, the
  // date_created or the last_modified time.  Neither pointer should be NULL.
  void ExpectSimilar(const TemplateURL* expected, const TemplateURL* actual);

  // Create an URL that appears to have been prepopulated, but won't be in the
  // current data. The caller owns the returned TemplateURL*.
  TemplateURL* CreatePreloadedTemplateURL(bool safe_for_autoreplace,
                                          int prepopulate_id);

  // Creates a TemplateURL with the same prepopulated id as a real prepopulated
  // item. The input number determines which prepopulated item. The caller is
  // responsible for owning the returned TemplateURL*.
  TemplateURL* CreateReplaceablePreloadedTemplateURL(
      bool safe_for_autoreplace,
      size_t index_offset_from_default,
      string16* prepopulated_display_url);

  // Verifies the behavior of when a preloaded url later gets changed.
  // Since the input is the offset from the default, when one passes in
  // 0, it tests the default. Passing in a number > 0 will verify what
  // happens when a preloaded url that is not the default gets updated.
  void TestLoadUpdatingPreloadedURL(size_t index_offset_from_default);

  // Helper methods to make calling TemplateURLServiceTestUtil methods less
  // visually noisy in the test code.
  void VerifyObserverCount(int expected_changed_count);
  void VerifyObserverFired();
  TemplateURLService* model() { return test_util_.model(); }

 protected:
  TemplateURLServiceTestUtil test_util_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceTest);
};

TemplateURLServiceTest::TemplateURLServiceTest() {
}

void TemplateURLServiceTest::SetUp() {
  test_util_.SetUp();
}

void TemplateURLServiceTest::TearDown() {
  test_util_.TearDown();
}

TemplateURL* TemplateURLServiceTest::AddKeywordWithDate(
    const std::string& short_name,
    const std::string& keyword,
    const std::string& url,
    const std::string& suggest_url,
    const std::string& alternate_url,
    const std::string& favicon_url,
    bool safe_for_autoreplace,
    const std::string& encodings,
    Time date_created,
    Time last_modified) {
  TemplateURLData data;
  data.short_name = UTF8ToUTF16(short_name);
  data.SetKeyword(UTF8ToUTF16(keyword));
  data.SetURL(url);
  data.suggestions_url = suggest_url;
  if (!alternate_url.empty())
    data.alternate_urls.push_back(alternate_url);
  data.favicon_url = GURL(favicon_url);
  data.safe_for_autoreplace = safe_for_autoreplace;
  base::SplitString(encodings, ';', &data.input_encodings);
  data.date_created = date_created;
  data.last_modified = last_modified;
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);
  EXPECT_NE(0, t_url->id());
  return t_url;
}

void TemplateURLServiceTest::AssertEquals(const TemplateURL& expected,
                                          const TemplateURL& actual) {
  ASSERT_EQ(expected.short_name(), actual.short_name());
  ASSERT_EQ(expected.keyword(), actual.keyword());
  ASSERT_EQ(expected.url(), actual.url());
  ASSERT_EQ(expected.suggestions_url(), actual.suggestions_url());
  ASSERT_EQ(expected.favicon_url(), actual.favicon_url());
  ASSERT_EQ(expected.alternate_urls(), actual.alternate_urls());
  ASSERT_EQ(expected.show_in_default_list(), actual.show_in_default_list());
  ASSERT_EQ(expected.safe_for_autoreplace(), actual.safe_for_autoreplace());
  ASSERT_EQ(expected.input_encodings(), actual.input_encodings());
  ASSERT_EQ(expected.id(), actual.id());
  ASSERT_EQ(expected.date_created(), actual.date_created());
  ASSERT_EQ(expected.last_modified(), actual.last_modified());
  ASSERT_EQ(expected.sync_guid(), actual.sync_guid());
  ASSERT_EQ(expected.search_terms_replacement_key(),
            actual.search_terms_replacement_key());
}

void TemplateURLServiceTest::ExpectSimilar(const TemplateURL* expected,
                                           const TemplateURL* actual) {
  ASSERT_TRUE(expected != NULL);
  ASSERT_TRUE(actual != NULL);
  EXPECT_EQ(expected->short_name(), actual->short_name());
  EXPECT_EQ(expected->keyword(), actual->keyword());
  EXPECT_EQ(expected->url(), actual->url());
  EXPECT_EQ(expected->suggestions_url(), actual->suggestions_url());
  EXPECT_EQ(expected->favicon_url(), actual->favicon_url());
  EXPECT_EQ(expected->alternate_urls(), actual->alternate_urls());
  EXPECT_EQ(expected->show_in_default_list(), actual->show_in_default_list());
  EXPECT_EQ(expected->safe_for_autoreplace(), actual->safe_for_autoreplace());
  EXPECT_EQ(expected->input_encodings(), actual->input_encodings());
  EXPECT_EQ(expected->search_terms_replacement_key(),
            actual->search_terms_replacement_key());
}

TemplateURL* TemplateURLServiceTest::CreatePreloadedTemplateURL(
    bool safe_for_autoreplace,
    int prepopulate_id) {
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("unittest");
  data.SetKeyword(ASCIIToUTF16("unittest"));
  data.SetURL("http://www.unittest.com/{searchTerms}");
  data.favicon_url = GURL("http://favicon.url");
  data.show_in_default_list = true;
  data.safe_for_autoreplace = safe_for_autoreplace;
  data.input_encodings.push_back("UTF-8");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.prepopulate_id = prepopulate_id;
  return new TemplateURL(test_util_.profile(), data);
}

TemplateURL* TemplateURLServiceTest::CreateReplaceablePreloadedTemplateURL(
    bool safe_for_autoreplace,
    size_t index_offset_from_default,
    string16* prepopulated_display_url) {
  ScopedVector<TemplateURL> prepopulated_urls;
  size_t default_search_provider_index = 0;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(test_util_.profile(),
      &prepopulated_urls.get(), &default_search_provider_index);
  EXPECT_LT(index_offset_from_default, prepopulated_urls.size());
  size_t prepopulated_index = (default_search_provider_index +
      index_offset_from_default) % prepopulated_urls.size();
  TemplateURL* t_url = CreatePreloadedTemplateURL(safe_for_autoreplace,
      prepopulated_urls[prepopulated_index]->prepopulate_id());
  *prepopulated_display_url =
      prepopulated_urls[prepopulated_index]->url_ref().DisplayURL();
  return t_url;
}

void TemplateURLServiceTest::TestLoadUpdatingPreloadedURL(
    size_t index_offset_from_default) {
  string16 prepopulated_url;
  TemplateURL* t_url = CreateReplaceablePreloadedTemplateURL(false,
      index_offset_from_default, &prepopulated_url);

  string16 original_url = t_url->url_ref().DisplayURL();
  std::string original_guid = t_url->sync_guid();
  EXPECT_NE(prepopulated_url, original_url);

  // Then add it to the model and save it all.
  test_util_.ChangeModelToLoadState();
  model()->Add(t_url);
  const TemplateURL* keyword_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(keyword_url != NULL);
  EXPECT_EQ(t_url, keyword_url);
  EXPECT_EQ(original_url, keyword_url->url_ref().DisplayURL());
  test_util_.BlockTillServiceProcessesRequests();

  // Now reload the model and verify that the merge updates the url, and
  // preserves the sync GUID.
  test_util_.ResetModel(true);
  keyword_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(keyword_url != NULL);
  EXPECT_EQ(prepopulated_url, keyword_url->url_ref().DisplayURL());
  EXPECT_EQ(original_guid, keyword_url->sync_guid());

  // Wait for any saves to finish.
  test_util_.BlockTillServiceProcessesRequests();

  // Reload the model to verify that change was saved correctly.
  test_util_.ResetModel(true);
  keyword_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(keyword_url != NULL);
  EXPECT_EQ(prepopulated_url, keyword_url->url_ref().DisplayURL());
  EXPECT_EQ(original_guid, keyword_url->sync_guid());
}

void TemplateURLServiceTest::VerifyObserverCount(int expected_changed_count) {
  EXPECT_EQ(expected_changed_count, test_util_.GetObserverCount());
  test_util_.ResetObserverCount();
}

void TemplateURLServiceTest::VerifyObserverFired() {
  EXPECT_LE(1, test_util_.GetObserverCount());
  test_util_.ResetObserverCount();
}


// Actual tests ---------------------------------------------------------------

TEST_F(TemplateURLServiceTest, Load) {
  test_util_.VerifyLoad();
}

TEST_F(TemplateURLServiceTest, AddUpdateRemove) {
  // Add a new TemplateURL.
  test_util_.VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("google");
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = true;
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.sync_guid = "00000000-0000-0000-0000-000000000001";
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("keyword"), GURL(),
                                         NULL));
  VerifyObserverCount(1);
  test_util_.BlockTillServiceProcessesRequests();
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(t_url->keyword()));
  // We need to make a second copy as the model takes ownership of |t_url| and
  // will delete it.  We have to do this after calling Add() since that gives
  // |t_url| its ID.
  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(t_url->profile(),
                                                     t_url->data()));

  // Reload the model to verify it was actually saved to the database.
  test_util_.ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(*cloned_url, *loaded_url);
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("keyword"), GURL(),
                                         NULL));

  // We expect the last_modified time to be updated to the present time on an
  // explicit reset. We have to set up the expectation here because ResetModel
  // resets the TimeProvider in the TemplateURLService.
  StrictMock<base::MockTimeProvider> mock_time;
  model()->set_time_provider(&base::MockTimeProvider::StaticNow);
  EXPECT_CALL(mock_time, Now()).WillOnce(Return(base::Time::FromDoubleT(1337)));

  // Mutate an element and verify it succeeded.
  model()->ResetTemplateURL(loaded_url, ASCIIToUTF16("a"), ASCIIToUTF16("b"),
                            "c");
  ASSERT_EQ(ASCIIToUTF16("a"), loaded_url->short_name());
  ASSERT_EQ(ASCIIToUTF16("b"), loaded_url->keyword());
  ASSERT_EQ("c", loaded_url->url());
  ASSERT_FALSE(loaded_url->safe_for_autoreplace());
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("keyword"), GURL(),
                                         NULL));
  ASSERT_FALSE(model()->CanReplaceKeyword(ASCIIToUTF16("b"), GURL(), NULL));
  cloned_url.reset(new TemplateURL(loaded_url->profile(), loaded_url->data()));
  test_util_.BlockTillServiceProcessesRequests();
  test_util_.ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("b"));
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(*cloned_url, *loaded_url);
  // We changed a TemplateURL in the service, so ensure that the time was
  // updated.
  ASSERT_EQ(base::Time::FromDoubleT(1337), loaded_url->last_modified());

  // Remove an element and verify it succeeded.
  model()->Remove(loaded_url);
  VerifyObserverCount(1);
  test_util_.ResetModel(true);
  ASSERT_EQ(initial_count, model()->GetTemplateURLs().size());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("b")) == NULL);
}

TEST_F(TemplateURLServiceTest, AddSameKeyword) {
  test_util_.VerifyLoad();

  AddKeywordWithDate(
      "first", "keyword", "http://test1", std::string(), std::string(),
      std::string(), true, "UTF-8", Time(), Time());
  VerifyObserverCount(1);

  // Test what happens when we try to add a TemplateURL with the same keyword as
  // one in the model.
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("second");
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://test2");
  data.safe_for_autoreplace = false;
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);

  // Because the old TemplateURL was replaceable and the new one wasn't, the new
  // one should have replaced the old.
  VerifyObserverCount(1);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(ASCIIToUTF16("second"), t_url->short_name());
  EXPECT_EQ(ASCIIToUTF16("keyword"), t_url->keyword());
  EXPECT_FALSE(t_url->safe_for_autoreplace());

  // Now try adding a replaceable TemplateURL.  This should just delete the
  // passed-in URL.
  data.short_name = ASCIIToUTF16("third");
  data.SetURL("http://test3");
  data.safe_for_autoreplace = true;
  model()->Add(new TemplateURL(test_util_.profile(), data));
  VerifyObserverCount(0);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(ASCIIToUTF16("second"), t_url->short_name());
  EXPECT_EQ(ASCIIToUTF16("keyword"), t_url->keyword());
  EXPECT_FALSE(t_url->safe_for_autoreplace());

  // Now try adding a non-replaceable TemplateURL again.  This should uniquify
  // the existing entry's keyword.
  data.short_name = ASCIIToUTF16("fourth");
  data.SetURL("http://test4");
  data.safe_for_autoreplace = false;
  TemplateURL* t_url2 = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url2);
  VerifyObserverCount(2);
  EXPECT_EQ(t_url2, model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(ASCIIToUTF16("fourth"), t_url2->short_name());
  EXPECT_EQ(ASCIIToUTF16("keyword"), t_url2->keyword());
  EXPECT_EQ(ASCIIToUTF16("second"), t_url->short_name());
  EXPECT_EQ(ASCIIToUTF16("test2"), t_url->keyword());
}

TEST_F(TemplateURLServiceTest, AddExtensionKeyword) {
  TemplateURL* original1 = AddKeywordWithDate(
      "replaceable", "keyword1", "http://test1", std::string(), std::string(),
      std::string(), true, "UTF-8", Time(), Time());
  TemplateURL* original2 = AddKeywordWithDate(
      "nonreplaceable", "keyword2", "http://test2", std::string(),
      std::string(), std::string(), false, "UTF-8", Time(), Time());
  TemplateURL* original3 = AddKeywordWithDate(
      "extension", "keyword3",
      std::string(extensions::kExtensionScheme) + "://test3", std::string(),
      std::string(), std::string(), false, "UTF-8", Time(), Time());

  // Add an extension keyword that conflicts with each of the above three
  // keywords.
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("test");
  data.SetKeyword(ASCIIToUTF16("keyword1"));
  data.SetURL(std::string(extensions::kExtensionScheme) + "://test4");
  data.safe_for_autoreplace = false;

  // Extension keywords should override replaceable keywords.
  TemplateURL* extension1 = new TemplateURL(test_util_.profile(), data);
  model()->Add(extension1);
  ASSERT_EQ(extension1,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword1")));
  model()->Remove(extension1);
  EXPECT_EQ(original1,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword1")));

  // They should not override non-replaceable keywords.
  data.SetKeyword(ASCIIToUTF16("keyword2"));
  TemplateURL* extension2 = new TemplateURL(test_util_.profile(), data);
  model()->Add(extension2);
  ASSERT_EQ(original2,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword2")));
  model()->Remove(original2);
  EXPECT_EQ(extension2,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword2")));

  // They should override extension keywords added earlier.
  data.SetKeyword(ASCIIToUTF16("keyword3"));
  TemplateURL* extension3 = new TemplateURL(test_util_.profile(), data);
  model()->Add(extension3);
  ASSERT_EQ(extension3,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword3")));
  model()->Remove(extension3);
  EXPECT_EQ(original3,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword3")));
}

TEST_F(TemplateURLServiceTest, AddSameKeywordWithExtensionPresent) {
  test_util_.VerifyLoad();

  // Similar to the AddSameKeyword test, but with an extension keyword masking a
  // replaceable TemplateURL.  We should still do correct conflict resolution
  // between the non-template URLs.
  AddKeywordWithDate(
      "replaceable", "keyword", "http://test1", std::string(),  std::string(),
      std::string(), true, "UTF-8", Time(), Time());
  TemplateURL* extension = AddKeywordWithDate(
      "extension", "keyword",
      std::string(extensions::kExtensionScheme) + "://test2", std::string(),
      std::string(), std::string(), false, "UTF-8", Time(), Time());

  // Adding another replaceable keyword should remove the existing one, but
  // leave the extension as the associated URL for this keyword.
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("name1");
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://test3");
  data.safe_for_autoreplace = true;
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);
  EXPECT_EQ(extension,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_TRUE(model()->GetTemplateURLForHost("test1") == NULL);
  EXPECT_EQ(t_url, model()->GetTemplateURLForHost("test3"));

  // Adding a nonreplaceable keyword should remove the existing replaceable
  // keyword and replace the extension as the associated URL for this keyword,
  // but not evict the extension from the service entirely.
  data.short_name = ASCIIToUTF16("name2");
  data.SetURL("http://test4");
  data.safe_for_autoreplace = false;
  TemplateURL* t_url2 = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url2);
  EXPECT_EQ(t_url2,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_TRUE(model()->GetTemplateURLForHost("test3") == NULL);
  // Note that extensions don't use host-based keywords, so we can't check that
  // the extension is still in the model using GetTemplateURLForHost(); and we
  // have to create a full-fledged Extension to use
  // GetTemplateURLForExtension(), which is annoying, so just grab all the URLs
  // from the model and search for |extension| within them.
  TemplateURLService::TemplateURLVector template_urls(
      model()->GetTemplateURLs());
  EXPECT_FALSE(std::find(template_urls.begin(), template_urls.end(),
                         extension) == template_urls.end());
}

TEST_F(TemplateURLServiceTest, GenerateKeyword) {
  ASSERT_EQ(ASCIIToUTF16("foo"),
            TemplateURLService::GenerateKeyword(GURL("http://foo")));
  // www. should be stripped.
  ASSERT_EQ(ASCIIToUTF16("foo"),
            TemplateURLService::GenerateKeyword(GURL("http://www.foo")));
  // Make sure we don't get a trailing '/'.
  ASSERT_EQ(ASCIIToUTF16("blah"),
            TemplateURLService::GenerateKeyword(GURL("http://blah/")));
  // Don't generate the empty string.
  ASSERT_EQ(ASCIIToUTF16("www"),
            TemplateURLService::GenerateKeyword(GURL("http://www.")));
}

TEST_F(TemplateURLServiceTest, GenerateSearchURL) {
  scoped_refptr<TestGenerateSearchURL> test_generate_search_url(
      new TestGenerateSearchURL(NULL));
  test_generate_search_url->RunTest();
  EXPECT_TRUE(test_generate_search_url->passed());
}

TEST_F(TemplateURLServiceTest, GenerateSearchURLUsingTermsData) {
  // Run the test for GenerateSearchURLUsingTermsData on the "IO" thread and
  // wait for it to finish.
  TestSearchTermsData search_terms_data("http://google.com/");
  scoped_refptr<TestGenerateSearchURL> test_generate_search_url(
      new TestGenerateSearchURL(&search_terms_data));

  test_util_.StartIOThread();
  BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)->PostTask(
          FROM_HERE, base::Bind(&TestGenerateSearchURL::RunTest,
                                test_generate_search_url.get()));
  TemplateURLServiceTestUtil::BlockTillIOThreadProcessesRequests();
  EXPECT_TRUE(test_generate_search_url->passed());
}

TEST_F(TemplateURLServiceTest, ClearBrowsingData_Keywords) {
  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

  // Nothing has been added.
  EXPECT_EQ(0U, model()->GetTemplateURLs().size());

  // Create one with a 0 time.
  AddKeywordWithDate("name1", "key1", "http://foo1", "http://suggest1",
                     std::string(), "http://icon1", true, "UTF-8;UTF-16",
                     Time(), Time());
  // Create one for now and +/- 1 day.
  AddKeywordWithDate("name2", "key2", "http://foo2", "http://suggest2",
                     std::string(),  "http://icon2", true, "UTF-8;UTF-16",
                     now - one_day, Time());
  AddKeywordWithDate("name3", "key3", "http://foo3", std::string(),
                     std::string(), std::string(), true, std::string(), now,
                     Time());
  AddKeywordWithDate("name4", "key4", "http://foo4", std::string(),
                     std::string(), std::string(), true, std::string(),
                     now + one_day, Time());
  // Try the other three states.
  AddKeywordWithDate("name5", "key5", "http://foo5", "http://suggest5",
                     std::string(), "http://icon5", false, "UTF-8;UTF-16", now,
                     Time());
  AddKeywordWithDate("name6", "key6", "http://foo6", "http://suggest6",
                     std::string(), "http://icon6", false, "UTF-8;UTF-16",
                     month_ago, Time());

  // We just added a few items, validate them.
  EXPECT_EQ(6U, model()->GetTemplateURLs().size());

  // Try removing from current timestamp. This should delete the one in the
  // future and one very recent one.
  model()->RemoveAutoGeneratedSince(now);
  EXPECT_EQ(4U, model()->GetTemplateURLs().size());

  // Try removing from two months ago. This should only delete items that are
  // auto-generated.
  model()->RemoveAutoGeneratedBetween(now - TimeDelta::FromDays(60), now);
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

TEST_F(TemplateURLServiceTest, ClearBrowsingData_KeywordsForOrigin) {
  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

  // Nothing has been added.
  EXPECT_EQ(0U, model()->GetTemplateURLs().size());

  // Create one for now and +/- 1 day.
  AddKeywordWithDate("name1", "key1", "http://foo1", "http://suggest1",
                     std::string(), "http://icon2", true, "UTF-8;UTF-16",
                     now - one_day, Time());
  AddKeywordWithDate("name2", "key2", "http://foo2", std::string(),
                     std::string(), std::string(), true, std::string(), now,
                     Time());
  AddKeywordWithDate("name3", "key3", "http://foo3", std::string(),
                     std::string(), std::string(), true, std::string(),
                     now + one_day, Time());

  // We just added a few items, validate them.
  EXPECT_EQ(3U, model()->GetTemplateURLs().size());

  // Try removing foo2. This should delete foo2, but leave foo1 and 3 untouched.
  model()->RemoveAutoGeneratedForOriginBetween(GURL("http://foo2"), month_ago,
      now + one_day);
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(ASCIIToUTF16("key3"), model()->GetTemplateURLs()[1]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());

  // Try removing foo1, but outside the range in which it was modified. It
  // should remain untouched.
  model()->RemoveAutoGeneratedForOriginBetween(GURL("http://foo1"), now,
      now + one_day);
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(ASCIIToUTF16("key3"), model()->GetTemplateURLs()[1]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());


  // Try removing foo3. This should delete foo3, but leave foo1 untouched.
  model()->RemoveAutoGeneratedForOriginBetween(GURL("http://foo3"), month_ago,
      now + one_day + one_day);
  EXPECT_EQ(1U, model()->GetTemplateURLs().size());
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
}

TEST_F(TemplateURLServiceTest, Reset) {
  // Add a new TemplateURL.
  test_util_.VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("google");
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);

  VerifyObserverCount(1);
  test_util_.BlockTillServiceProcessesRequests();

  StrictMock<base::MockTimeProvider> mock_time;
  model()->set_time_provider(&base::MockTimeProvider::StaticNow);
  EXPECT_CALL(mock_time, Now()).WillOnce(Return(base::Time::FromDoubleT(1337)));

  // Reset the short name, keyword, url and make sure it takes.
  const string16 new_short_name(ASCIIToUTF16("a"));
  const string16 new_keyword(ASCIIToUTF16("b"));
  const std::string new_url("c");
  model()->ResetTemplateURL(t_url, new_short_name, new_keyword, new_url);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(new_keyword, t_url->keyword());
  ASSERT_EQ(new_url, t_url->url());

  // Make sure the mappings in the model were updated.
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(new_keyword));
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")) == NULL);

  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(t_url->profile(),
                                                     t_url->data()));

  // Reload the model from the database and make sure the change took.
  test_util_.ResetModel(true);
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* read_url = model()->GetTemplateURLForKeyword(new_keyword);
  ASSERT_TRUE(read_url);
  AssertEquals(*cloned_url, *read_url);
  ASSERT_EQ(base::Time::FromDoubleT(1337), read_url->last_modified());
}

TEST_F(TemplateURLServiceTest, DefaultSearchProvider) {
  // Add a new TemplateURL.
  test_util_.VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURL* t_url = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16", Time(), Time());
  test_util_.ResetObserverCount();

  model()->SetDefaultSearchProvider(t_url);
  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());
  ASSERT_TRUE(t_url->safe_for_autoreplace());
  ASSERT_TRUE(t_url->show_in_default_list());

  // Setting the default search provider should have caused notification.
  VerifyObserverCount(1);
  test_util_.BlockTillServiceProcessesRequests();

  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(t_url->profile(),
                                                     t_url->data()));

  // Make sure when we reload we get a default search provider.
  test_util_.ResetModel(true);
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(*cloned_url, *model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceTest, CantReplaceWithSameKeyword) {
  test_util_.ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"), GURL(), NULL));
  TemplateURL* t_url = AddKeywordWithDate(
      "name1", "foo", "http://foo1", "http://sugg1", std::string(),
      "http://icon1", true, "UTF-8;UTF-16", Time(), Time());

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"),
                                         GURL("http://foo2"), NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                            t_url->url());

  ASSERT_FALSE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"),
                                          GURL("http://foo2"), NULL));
}

TEST_F(TemplateURLServiceTest, CantReplaceWithSameHosts) {
  test_util_.ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("foo"),
                                         GURL("http://foo.com"), NULL));
  TemplateURL* t_url = AddKeywordWithDate(
      "name1", "foo", "http://foo.com", "http://sugg1", std::string(),
      "http://icon1", true, "UTF-8;UTF-16", Time(), Time());

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanReplaceKeyword(ASCIIToUTF16("bar"),
                                         GURL("http://foo.com"), NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                            t_url->url());

  ASSERT_FALSE(model()->CanReplaceKeyword(ASCIIToUTF16("bar"),
                                          GURL("http://foo.com"), NULL));
}

TEST_F(TemplateURLServiceTest, HasDefaultSearchProvider) {
  // We should have a default search provider even if we haven't loaded.
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Now force the model to load and make sure we still have a default.
  test_util_.VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceTest, DefaultSearchProviderLoadedFromPrefs) {
  test_util_.VerifyLoad();

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("a");
  data.safe_for_autoreplace = true;
  data.SetURL("http://url/{searchTerms}");
  data.suggestions_url = "http://url2";
  data.instant_url = "http://instant";
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);
  const TemplateURLID id = t_url->id();

  model()->SetDefaultSearchProvider(t_url);
  test_util_.BlockTillServiceProcessesRequests();
  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(t_url->profile(),
                                                     t_url->data()));

  // Reset the model and don't load it. The template url we set as the default
  // should be pulled from prefs now.
  test_util_.ResetModel(false);

  // NOTE: This doesn't use AssertEquals as only a subset of the TemplateURLs
  // value are persisted to prefs.
  const TemplateURL* default_turl = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_turl);
  EXPECT_EQ(ASCIIToUTF16("a"), default_turl->short_name());
  EXPECT_EQ("http://url/{searchTerms}", default_turl->url());
  EXPECT_EQ("http://url2", default_turl->suggestions_url());
  EXPECT_EQ("http://instant", default_turl->instant_url());
  EXPECT_EQ(id, default_turl->id());

  // Now do a load and make sure the default search provider really takes.
  test_util_.VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(*cloned_url, *model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceTest, BuildQueryTerms) {
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
    TemplateURLService::QueryTerms terms;
    ASSERT_EQ(data[i].result,
              TemplateURLService::BuildQueryTerms(GURL(data[i].url), &terms));
    if (data[i].result) {
      std::vector<std::string> keys;
      std::vector<std::string> values;
      base::SplitString(data[i].keys, ';', &keys);
      base::SplitString(data[i].values, ';', &values);
      ASSERT_TRUE(keys.size() == values.size());
      ASSERT_EQ(keys.size(), terms.size());
      for (size_t j = 0; j < keys.size(); ++j) {
        TemplateURLService::QueryTerms::iterator term_iterator =
            terms.find(keys[j]);
        ASSERT_TRUE(term_iterator != terms.end());
        ASSERT_EQ(values[j], term_iterator->second);
      }
    }
  }
}

TEST_F(TemplateURLServiceTest, UpdateKeywordSearchTermsForURL) {
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
    { "http://x/foo#query=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?q=b#query=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?q=b#q=xx", ASCIIToUTF16("b") },
    { "http://x/foo?query=b#q=xx", string16() },
  };

  test_util_.ChangeModelToLoadState();
  AddKeywordWithDate("name", "x", "http://x/foo?q={searchTerms}",
                     "http://sugg1", "http://x/foo#query={searchTerms}",
                     "http://icon1", false, "UTF-8;UTF-16", Time(), Time());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLVisitedDetails details;
    details.row = history::URLRow(GURL(data[i].url));
    details.transition = content::PageTransitionFromInt(0);
    model()->UpdateKeywordSearchTermsForURL(details);
    EXPECT_EQ(data[i].term, test_util_.GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLServiceTest, DontUpdateKeywordSearchForNonReplaceable) {
  struct TestData {
    const std::string url;
  } data[] = {
    { "http://foo/" },
    { "http://x/bar?q=xx" },
    { "http://x/foo?y=xx" },
  };

  test_util_.ChangeModelToLoadState();
  AddKeywordWithDate("name", "x", "http://x/foo", "http://sugg1", std::string(),
                     "http://icon1", false, "UTF-8;UTF-16", Time(), Time());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLVisitedDetails details;
    details.row = history::URLRow(GURL(data[i].url));
    details.transition = content::PageTransitionFromInt(0);
    model()->UpdateKeywordSearchTermsForURL(details);
    ASSERT_EQ(string16(), test_util_.GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLServiceTest, ChangeGoogleBaseValue) {
  // NOTE: Do not do a VerifyLoad() here as it will load the prepopulate data,
  // which also has a {google:baseURL} keyword in it, which will confuse this
  // test.
  test_util_.ChangeModelToLoadState();
  test_util_.SetGoogleBaseURL(GURL("http://google.com/"));
  const TemplateURL* t_url = AddKeywordWithDate(
      "name", "google.com", "{google:baseURL}?q={searchTerms}", "http://sugg1",
      std::string(), "http://icon1", false, "UTF-8;UTF-16", Time(), Time());
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("google.com"));
  EXPECT_EQ("google.com", t_url->url_ref().GetHost());
  EXPECT_EQ(ASCIIToUTF16("google.com"), t_url->keyword());

  // Change the Google base url.
  test_util_.ResetObserverCount();
  test_util_.SetGoogleBaseURL(GURL("http://google.co.uk/"));
  VerifyObserverCount(1);

  // Make sure the host->TemplateURL map was updated appropriately.
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("google.co.uk"));
  EXPECT_TRUE(model()->GetTemplateURLForHost("google.com") == NULL);
  EXPECT_EQ("google.co.uk", t_url->url_ref().GetHost());
  EXPECT_EQ(ASCIIToUTF16("google.co.uk"), t_url->keyword());
  EXPECT_EQ("http://google.co.uk/?q=x", t_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("x"))));

  // Now add a manual entry and then change the Google base URL such that the
  // autogenerated Google search keyword would conflict.
  TemplateURL* manual = AddKeywordWithDate(
    "manual", "google.de", "http://google.de/search?q={searchTerms}",
    std::string(), std::string(), std::string(), false, "UTF-8", Time(),
    Time());
  test_util_.SetGoogleBaseURL(GURL("http://google.de"));

  // Verify that the manual entry is untouched, and the autogenerated keyword
  // has not changed.
  ASSERT_EQ(manual,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("google.de")));
  EXPECT_EQ("google.de", manual->url_ref().GetHost());
  ASSERT_EQ(t_url,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("google.co.uk")));
  EXPECT_EQ("google.de", t_url->url_ref().GetHost());
  EXPECT_EQ(ASCIIToUTF16("google.co.uk"), t_url->keyword());

  // Change the base URL again and verify that the autogenerated keyword follows
  // even though it didn't match the base URL, while the manual entry is still
  // untouched.
  test_util_.SetGoogleBaseURL(GURL("http://google.fr/"));
  ASSERT_EQ(manual, model()->GetTemplateURLForHost("google.de"));
  EXPECT_EQ("google.de", manual->url_ref().GetHost());
  EXPECT_EQ(ASCIIToUTF16("google.de"), manual->keyword());
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("google.fr"));
  EXPECT_TRUE(model()->GetTemplateURLForHost("google.co.uk") == NULL);
  EXPECT_EQ("google.fr", t_url->url_ref().GetHost());
  EXPECT_EQ(ASCIIToUTF16("google.fr"), t_url->keyword());
}

// Make sure TemplateURLService generates a KEYWORD_GENERATED visit for
// KEYWORD visits.
TEST_F(TemplateURLServiceTest, GenerateVisitOnKeyword) {
  test_util_.VerifyLoad();
  test_util_.profile()->CreateHistoryService(true, false);

  // Create a keyword.
  TemplateURL* t_url = AddKeywordWithDate(
      "keyword", "keyword", "http://foo.com/foo?query={searchTerms}",
      "http://sugg1", std::string(), "http://icon1", true, "UTF-8;UTF-16",
      base::Time::Now(), base::Time::Now());

  // Add a visit that matches the url of the keyword.
  HistoryService* history =
      HistoryServiceFactory::GetForProfile(test_util_.profile(),
                                           Profile::EXPLICIT_ACCESS);
  history->AddPage(
      GURL(t_url->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("blah")))),
      base::Time::Now(), NULL, 0, GURL(), history::RedirectList(),
      content::PAGE_TRANSITION_KEYWORD, history::SOURCE_BROWSED, false);

  // Wait for history to finish processing the request.
  test_util_.profile()->BlockUntilHistoryProcessesPendingRequests();

  // Query history for the generated url.
  CancelableRequestConsumer consumer;
  QueryHistoryCallbackImpl callback;
  history->QueryURL(GURL("http://keyword"), true, &consumer,
      base::Bind(&QueryHistoryCallbackImpl::Callback,
                 base::Unretained(&callback)));

  // Wait for the request to be processed.
  test_util_.profile()->BlockUntilHistoryProcessesPendingRequests();

  // And make sure the url and visit were added.
  EXPECT_TRUE(callback.success);
  EXPECT_NE(0, callback.row.id());
  ASSERT_EQ(1U, callback.visits.size());
  EXPECT_EQ(content::PAGE_TRANSITION_KEYWORD_GENERATED,
      content::PageTransitionStripQualifier(callback.visits[0].transition));
}

// Make sure that the load routine deletes prepopulated engines that no longer
// exist in the prepopulate data.
TEST_F(TemplateURLServiceTest, LoadDeletesUnusedProvider) {
  // Create a preloaded template url. Add it to a loaded model and wait for the
  // saves to finish.
  TemplateURL* t_url = CreatePreloadedTemplateURL(true, 999999);
  test_util_.ChangeModelToLoadState();
  model()->Add(t_url);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) != NULL);
  test_util_.BlockTillServiceProcessesRequests();

  // Ensure that merging clears this engine.
  test_util_.ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) == NULL);

  // Wait for any saves to finish.
  test_util_.BlockTillServiceProcessesRequests();

  // Reload the model to verify that the database was updated as a result of the
  // merge.
  test_util_.ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) == NULL);
}

// Make sure that load routine doesn't delete prepopulated engines that no
// longer exist in the prepopulate data if it has been modified by the user.
TEST_F(TemplateURLServiceTest, LoadRetainsModifiedProvider) {
  // Create a preloaded template url and add it to a loaded model.
  TemplateURL* t_url = CreatePreloadedTemplateURL(false, 999999);
  test_util_.ChangeModelToLoadState();
  model()->Add(t_url);

  // Do the copy after t_url is added so that the id is set.
  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(t_url->profile(),
                                                     t_url->data()));
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")));

  // Wait for any saves to finish.
  test_util_.BlockTillServiceProcessesRequests();

  // Ensure that merging won't clear it if the user has edited it.
  test_util_.ResetModel(true);
  const TemplateURL* url_for_unittest =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(url_for_unittest != NULL);
  AssertEquals(*cloned_url, *url_for_unittest);

  // Wait for any saves to finish.
  test_util_.BlockTillServiceProcessesRequests();

  // Reload the model to verify that save/reload retains the item.
  test_util_.ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) != NULL);
}

// Make sure that load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it has been modified by the user.
TEST_F(TemplateURLServiceTest, LoadSavesPrepopulatedDefaultSearchProvider) {
  test_util_.VerifyLoad();
  // Verify that the default search provider is set to something.
  TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search != NULL);
  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(default_search->profile(),
                                                     default_search->data()));

  // Wait for any saves to finish.
  test_util_.BlockTillServiceProcessesRequests();

  // Reload the model and check that the default search provider
  // was properly saved.
  test_util_.ResetModel(true);
  default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search != NULL);
  AssertEquals(*cloned_url, *default_search);
}

TEST_F(TemplateURLServiceTest, FindNewDefaultSearchProvider) {
  // Ensure that if our service is initially empty, we don't initial have a
  // valid new DSP.
  EXPECT_FALSE(model()->FindNewDefaultSearchProvider());

  // Add a few entries with searchTerms, but ensure only the last one is in the
  // default list.
  AddKeywordWithDate("name1", "key1", "http://foo1/{searchTerms}",
                     "http://sugg1", std::string(), "http://icon1", true,
                     "UTF-8;UTF-16", Time(), Time());
  AddKeywordWithDate("name2", "key2", "http://foo2/{searchTerms}",
                     "http://sugg2", std::string(), "http://icon1", true,
                     "UTF-8;UTF-16", Time(), Time());
  AddKeywordWithDate("name3", "key3", "http://foo1/{searchTerms}",
                     "http://sugg3", std::string(), "http://icon3", true,
                     "UTF-8;UTF-16", Time(), Time());
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("valid");
  data.SetKeyword(ASCIIToUTF16("validkeyword"));
  data.SetURL("http://valid/{searchTerms}");
  data.favicon_url = GURL("http://validicon");
  data.show_in_default_list = true;
  TemplateURL* valid_turl(new TemplateURL(test_util_.profile(), data));
  model()->Add(valid_turl);
  EXPECT_EQ(4U, model()->GetTemplateURLs().size());

  // Request a new DSP from the service and only expect the valid one.
  TemplateURL* new_default = model()->FindNewDefaultSearchProvider();
  ASSERT_TRUE(new_default);
  EXPECT_EQ(valid_turl, new_default);

  // Remove the default we received and ensure that the service returns NULL.
  model()->Remove(new_default);
  EXPECT_FALSE(model()->FindNewDefaultSearchProvider());
}

// Make sure that the load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it is the default search provider.
TEST_F(TemplateURLServiceTest, LoadRetainsDefaultProvider) {
  // Set the default search provider to a preloaded template url which
  // is not in the current set of preloaded template urls and save
  // the result.
  TemplateURL* t_url = CreatePreloadedTemplateURL(true, 999999);
  test_util_.ChangeModelToLoadState();
  model()->Add(t_url);
  model()->SetDefaultSearchProvider(t_url);
  // Do the copy after t_url is added and set as default so that its
  // internal state is correct.
  scoped_ptr<TemplateURL> cloned_url(new TemplateURL(t_url->profile(),
                                                     t_url->data()));

  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")));
  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());
  test_util_.BlockTillServiceProcessesRequests();

  // Ensure that merging won't clear the prepopulated template url
  // which is no longer present if it's the default engine.
  test_util_.ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
    ASSERT_TRUE(keyword_url != NULL);
    AssertEquals(*cloned_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }

  // Wait for any saves to finish.
  test_util_.BlockTillServiceProcessesRequests();

  // Reload the model to verify that the update was saved.
  test_util_.ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
    ASSERT_TRUE(keyword_url != NULL);
    AssertEquals(*cloned_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }
}

// Make sure that the load routine updates the url of a preexisting
// default search engine provider and that the result is saved correctly.
TEST_F(TemplateURLServiceTest, LoadUpdatesDefaultSearchURL) {
  TestLoadUpdatingPreloadedURL(0);
}

// Make sure that the load routine updates the url of a preexisting
// non-default search engine provider and that the result is saved correctly.
TEST_F(TemplateURLServiceTest, LoadUpdatesSearchURL) {
  TestLoadUpdatingPreloadedURL(1);
}

// Make sure that the load routine sets a default search provider if it was
// missing and not managed.
TEST_F(TemplateURLServiceTest, LoadEnsuresDefaultSearchProviderExists) {
  // Force the model to load and make sure we have a default search provider.
  test_util_.VerifyLoad();
  TemplateURL* old_default = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(old_default);

  // Now remove it.
  model()->SetDefaultSearchProvider(NULL);
  model()->Remove(old_default);
  test_util_.BlockTillServiceProcessesRequests();

  EXPECT_FALSE(model()->GetDefaultSearchProvider());

  // Reset the model and load it. There should be a default search provider.
  test_util_.ResetModel(true);

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->GetDefaultSearchProvider()->SupportsReplacement());

  // Make default search provider unusable (no search terms).
  model()->ResetTemplateURL(model()->GetDefaultSearchProvider(),
                            ASCIIToUTF16("test"), ASCIIToUTF16("test"),
                            "http://example.com/");
  test_util_.BlockTillServiceProcessesRequests();

  // Reset the model and load it. There should be a usable default search
  // provider.
  test_util_.ResetModel(true);

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->GetDefaultSearchProvider()->SupportsReplacement());
}

// Simulates failing to load the webdb and makes sure the default search
// provider is valid.
TEST_F(TemplateURLServiceTest, FailedInit) {
  test_util_.VerifyLoad();

  test_util_.ClearModel();
  scoped_refptr<WebDataService> web_service =
      WebDataService::FromBrowserContext(test_util_.profile());
  web_service->ShutdownDatabase();

  test_util_.ResetModel(false);
  model()->Load();
  test_util_.BlockTillServiceProcessesRequests();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

// Verifies that if the default search URL preference is managed, we report
// the default search as managed.  Also check that we are getting the right
// values.
TEST_F(TemplateURLServiceTest, TestManagedDefaultSearch) {
  test_util_.VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  test_util_.ResetObserverCount();

  // Set a regular default search provider.
  TemplateURL* regular_default = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16", Time(), Time());
  VerifyObserverCount(1);
  model()->SetDefaultSearchProvider(regular_default);
  // Adding the URL and setting the default search provider should have caused
  // notifications.
  VerifyObserverCount(1);
  EXPECT_FALSE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Set a managed preference that establishes a default search provider.
  const char kName[] = "test1";
  const char kKeyword[] = "test.com";
  const char kSearchURL[] = "http://test.com/search?t={searchTerms}";
  const char kIconURL[] = "http://test.com/icon.jpg";
  const char kEncodings[] = "UTF-16;UTF-32";
  const char kAlternateURL[] = "http://test.com/search#t={searchTerms}";
  const char kSearchTermsReplacementKey[] = "espv";
  test_util_.SetManagedDefaultSearchPreferences(true, kName, kKeyword,
      kSearchURL, std::string(), kIconURL, kEncodings, kAlternateURL,
      kSearchTermsReplacementKey);
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 2, model()->GetTemplateURLs().size());

  // Verify that the default manager we are getting is the managed one.
  TemplateURLData data;
  data.short_name = ASCIIToUTF16(kName);
  data.SetKeyword(ASCIIToUTF16(kKeyword));
  data.SetURL(kSearchURL);
  data.favicon_url = GURL(kIconURL);
  data.show_in_default_list = true;
  base::SplitString(kEncodings, ';', &data.input_encodings);
  data.alternate_urls.push_back(kAlternateURL);
  data.search_terms_replacement_key = kSearchTermsReplacementKey;
  Profile* profile = test_util_.profile();
  scoped_ptr<TemplateURL> expected_managed_default1(new TemplateURL(profile,
                                                                    data));
  const TemplateURL* actual_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default1.get(), actual_managed_default);
  EXPECT_TRUE(actual_managed_default->show_in_default_list());

  // Update the managed preference and check that the model has changed.
  const char kNewName[] = "test2";
  const char kNewKeyword[] = "other.com";
  const char kNewSearchURL[] = "http://other.com/search?t={searchTerms}";
  const char kNewSuggestURL[] = "http://other.com/suggest?t={searchTerms}";
  test_util_.SetManagedDefaultSearchPreferences(true, kNewName, kNewKeyword,
      kNewSearchURL, kNewSuggestURL, std::string(), std::string(),
      std::string(), std::string());
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 2, model()->GetTemplateURLs().size());

  // Verify that the default manager we are now getting is the correct one.
  TemplateURLData data2;
  data2.short_name = ASCIIToUTF16(kNewName);
  data2.SetKeyword(ASCIIToUTF16(kNewKeyword));
  data2.SetURL(kNewSearchURL);
  data2.suggestions_url = kNewSuggestURL;
  data2.show_in_default_list = true;
  scoped_ptr<TemplateURL> expected_managed_default2(new TemplateURL(profile,
                                                                    data2));
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default2.get(), actual_managed_default);
  EXPECT_EQ(actual_managed_default->show_in_default_list(), true);

  // Remove all the managed prefs and check that we are no longer managed.
  test_util_.RemoveManagedDefaultSearchPreferences();
  VerifyObserverFired();
  EXPECT_FALSE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // The default should now be the first URL added
  const TemplateURL* actual_final_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(model()->GetTemplateURLs()[0], actual_final_managed_default);
  EXPECT_EQ(actual_final_managed_default->show_in_default_list(), true);

  // Disable the default search provider through policy.
  test_util_.SetManagedDefaultSearchPreferences(false, std::string(),
      std::string(), std::string(), std::string(), std::string(),
      std::string(), std::string(), std::string());
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_TRUE(NULL == model()->GetDefaultSearchProvider());
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Re-enable it.
  test_util_.SetManagedDefaultSearchPreferences(true, kName, kKeyword,
      kSearchURL, std::string(), kIconURL, kEncodings, kAlternateURL,
      kSearchTermsReplacementKey);
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 2, model()->GetTemplateURLs().size());

  // Verify that the default manager we are getting is the managed one.
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default1.get(), actual_managed_default);
  EXPECT_EQ(actual_managed_default->show_in_default_list(), true);

  // Clear the model and disable the default search provider through policy.
  // Verify that there is no default search provider after loading the model.
  // This checks against regressions of http://crbug.com/67180

  // First, remove the preferences, reset the model, and set a default.
  test_util_.RemoveManagedDefaultSearchPreferences();
  test_util_.ResetModel(true);
  TemplateURL* new_default =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("key1"));
  ASSERT_FALSE(new_default == NULL);
  model()->SetDefaultSearchProvider(new_default);
  EXPECT_EQ(new_default, model()->GetDefaultSearchProvider());

  // Now reset the model again but load it after setting the preferences.
  test_util_.ResetModel(false);
  test_util_.SetManagedDefaultSearchPreferences(false, std::string(),
      std::string(), std::string(), std::string(), std::string(),
      std::string(), std::string(), std::string());
  test_util_.VerifyLoad();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_TRUE(model()->GetDefaultSearchProvider() == NULL);
}

// Test that if we load a TemplateURL with an empty GUID, the load process
// assigns it a newly generated GUID.
TEST_F(TemplateURLServiceTest, PatchEmptySyncGUID) {
  // Add a new TemplateURL.
  test_util_.VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("google");
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://www.google.com/foo/bar");
  data.sync_guid.clear();
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);

  VerifyObserverCount(1);
  test_util_.BlockTillServiceProcessesRequests();
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Reload the model to verify it was actually saved to the database and
  // assigned a new GUID when brought back.
  test_util_.ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_FALSE(loaded_url == NULL);
  ASSERT_FALSE(loaded_url->sync_guid().empty());
}

// Test that if we load a TemplateURL with duplicate input encodings, the load
// process de-dupes them.
TEST_F(TemplateURLServiceTest, DuplicateInputEncodings) {
  // Add a new TemplateURL.
  test_util_.VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("google");
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://www.google.com/foo/bar");
  std::vector<std::string> encodings;
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("UTF-16");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("Big5");
  data.input_encodings.push_back("UTF-16");
  data.input_encodings.push_back("Big5");
  data.input_encodings.push_back("Windows-1252");
  TemplateURL* t_url = new TemplateURL(test_util_.profile(), data);
  model()->Add(t_url);

  VerifyObserverCount(1);
  test_util_.BlockTillServiceProcessesRequests();
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_TRUE(loaded_url != NULL);
  EXPECT_EQ(8U, loaded_url->input_encodings().size());

  // Reload the model to verify it was actually saved to the database and the
  // duplicate encodings were removed.
  test_util_.ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_FALSE(loaded_url == NULL);
  EXPECT_EQ(4U, loaded_url->input_encodings().size());
}
