// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

namespace {

// Test fixtures -------------------------------------------------------------

class AutomaticProfileResetterDelegateTest : public testing::Test {
 protected:
  AutomaticProfileResetterDelegateTest() {}
  virtual ~AutomaticProfileResetterDelegateTest() {}

  virtual void SetUp() OVERRIDE {
    resetter_delegate_.reset(new AutomaticProfileResetterDelegateImpl(NULL));
  }

  virtual void TearDown() OVERRIDE { resetter_delegate_.reset(); }

  AutomaticProfileResetterDelegate* resetter_delegate() {
    return resetter_delegate_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<AutomaticProfileResetterDelegate> resetter_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateTest);
};

class AutomaticProfileResetterDelegateTestTemplateURLs : public testing::Test {
 protected:
  AutomaticProfileResetterDelegateTestTemplateURLs() {}
  virtual ~AutomaticProfileResetterDelegateTestTemplateURLs() {}

  virtual void SetUp() OVERRIDE {
    test_util_.SetUp();
    resetter_delegate_.reset(
        new AutomaticProfileResetterDelegateImpl(test_util_.model()));
  }

  virtual void TearDown() OVERRIDE {
    resetter_delegate_.reset();
    test_util_.TearDown();
  }

  TestingProfile* profile() { return test_util_.profile(); }

  AutomaticProfileResetterDelegate* resetter_delegate() {
    return resetter_delegate_.get();
  }

  scoped_ptr<TemplateURL> CreateTestTemplateURL() {
    TemplateURLData data;

    data.SetURL("http://example.com/search?q={searchTerms}");
    data.suggestions_url = "http://example.com/suggest?q={searchTerms}";
    data.instant_url = "http://example.com/instant?q={searchTerms}";
    data.image_url = "http://example.com/image?q={searchTerms}";
    data.search_url_post_params = "search-post-params";
    data.suggestions_url_post_params = "suggest-post-params";
    data.instant_url_post_params = "instant-post-params";
    data.image_url_post_params = "image-post-params";

    data.favicon_url = GURL("http://example.com/favicon.ico");
    data.new_tab_url = "http://example.com/newtab.html";
    data.alternate_urls.push_back("http://example.com/s?q={searchTerms}");

    data.short_name = base::ASCIIToUTF16("name");
    data.SetKeyword(base::ASCIIToUTF16("keyword"));
    data.search_terms_replacement_key = "search-terms-replacment-key";
    data.prepopulate_id = 42;
    data.input_encodings.push_back("UTF-8");
    data.safe_for_autoreplace = true;

    return scoped_ptr<TemplateURL>(new TemplateURL(profile(), data));
  }

  TemplateURLServiceTestUtil test_util_;

 private:
  scoped_ptr<AutomaticProfileResetterDelegate> resetter_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateTestTemplateURLs);
};

// Helper classes and functions ----------------------------------------------

// Returns the details of the default search provider from |prefs| in a format
// suitable for usage as |expected_details| in ExpectDetailsMatch().
scoped_ptr<base::DictionaryValue> GetDefaultSearchProviderDetailsFromPrefs(
    const PrefService* prefs) {
  const char kDefaultSearchProviderPrefix[] = "default_search_provider";
  scoped_ptr<base::DictionaryValue> pref_values_with_path_expansion(
      prefs->GetPreferenceValues());
  const base::DictionaryValue* dsp_details = NULL;
  EXPECT_TRUE(pref_values_with_path_expansion->GetDictionary(
      kDefaultSearchProviderPrefix, &dsp_details));
  return scoped_ptr<base::DictionaryValue>(
      dsp_details ? dsp_details->DeepCopy() : new base::DictionaryValue);
}

// Verifies that the |details| of a search engine as provided by the delegate
// are correct in comparison to the |expected_details| coming from the Prefs.
void ExpectDetailsMatch(const base::DictionaryValue& expected_details,
                        const base::DictionaryValue& details) {
  for (base::DictionaryValue::Iterator it(expected_details); !it.IsAtEnd();
       it.Advance()) {
    SCOPED_TRACE(testing::Message("Key: ") << it.key());
    if (it.key() == "enabled" || it.key() == "synced_guid") {
      // These attributes should not be present.
      EXPECT_FALSE(details.HasKey(it.key()));
      continue;
    }
    const base::Value* expected_value = &it.value();
    const base::Value* actual_value = NULL;
    ASSERT_TRUE(details.Get(it.key(), &actual_value));
    if (it.key() == "id") {
      // Ignore ID as it is dynamically assigned by the TemplateURLService.
    } else if (it.key() == "encodings") {
      // Encoding list is stored in Prefs as a single string with tokens
      // delimited by semicolons.
      std::string expected_encodings;
      ASSERT_TRUE(expected_value->GetAsString(&expected_encodings));
      const base::ListValue* actual_encodings_list = NULL;
      ASSERT_TRUE(actual_value->GetAsList(&actual_encodings_list));
      std::vector<std::string> actual_encodings_vector;
      for (base::ListValue::const_iterator it = actual_encodings_list->begin();
           it != actual_encodings_list->end(); ++it) {
        std::string encoding;
        ASSERT_TRUE((*it)->GetAsString(&encoding));
        actual_encodings_vector.push_back(encoding);
      }
      EXPECT_EQ(expected_encodings, JoinString(actual_encodings_vector, ';'));
    } else {
      // Everything else is the same format.
      EXPECT_TRUE(actual_value->Equals(expected_value));
    }
  }
}

class MockCallbackTarget {
 public:
  MockCallbackTarget() {}
  ~MockCallbackTarget() {}

  MOCK_CONST_METHOD0(Run, void(void));

  base::Closure CreateClosure() {
    return base::Closure(
        base::Bind(&MockCallbackTarget::Run, base::Unretained(this)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallbackTarget);
};

// Tests ---------------------------------------------------------------------

TEST_F(AutomaticProfileResetterDelegateTest,
       TriggerAndWaitOnModuleEnumeration) {
  testing::StrictMock<MockCallbackTarget> mock_target;

  // Expect ready_callback to be called just after the modules have been
  // enumerated. Fail if it is not called, or called too early.
  resetter_delegate()->RequestCallbackWhenLoadedModulesAreEnumerated(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->EnumerateLoadedModulesIfNeeded();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately when the modules have
  // already been enumerated.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenLoadedModulesAreEnumerated(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

#if defined(OS_WIN)
  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately even when the modules had
  // already been enumerated when the delegate was constructed.
  scoped_ptr<AutomaticProfileResetterDelegate> late_resetter_delegate(
      new AutomaticProfileResetterDelegateImpl(NULL));

  EXPECT_CALL(mock_target, Run());
  late_resetter_delegate->RequestCallbackWhenLoadedModulesAreEnumerated(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
#endif
}

TEST_F(AutomaticProfileResetterDelegateTest, GetLoadedModuleNameDigests) {
  resetter_delegate()->EnumerateLoadedModulesIfNeeded();
  base::RunLoop().RunUntilIdle();
  scoped_ptr<base::ListValue> module_name_digests(
      resetter_delegate()->GetLoadedModuleNameDigests());

  // Just verify that each element looks like an MD5 hash in hexadecimal, and
  // also that we have at least one element on Win.
  ASSERT_TRUE(module_name_digests);
  for (base::ListValue::const_iterator it = module_name_digests->begin();
       it != module_name_digests->end(); ++it) {
    std::string digest_hex;
    std::vector<uint8> digest_raw;

    ASSERT_TRUE((*it)->GetAsString(&digest_hex));
    ASSERT_TRUE(base::HexStringToBytes(digest_hex, &digest_raw));
    EXPECT_EQ(16u, digest_raw.size());
  }
#if defined(OS_WIN)
  EXPECT_LE(1u, module_name_digests->GetSize());
#endif
}

TEST_F(AutomaticProfileResetterDelegateTestTemplateURLs,
       LoadAndWaitOnTemplateURLService) {
  testing::StrictMock<MockCallbackTarget> mock_target;

  // Expect ready_callback to be called just after the template URL service gets
  // initialized. Fail if it is not called, or called too early.
  resetter_delegate()->RequestCallbackWhenTemplateURLServiceIsLoaded(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->LoadTemplateURLServiceIfNeeded();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately when the template URL
  // service is already initialized.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenTemplateURLServiceIsLoaded(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately even when the template URL
  // service had already been initialized when the delegate was constructed.
  scoped_ptr<AutomaticProfileResetterDelegate> late_resetter_delegate(
      new AutomaticProfileResetterDelegateImpl(test_util_.model()));

  EXPECT_CALL(mock_target, Run());
  late_resetter_delegate->RequestCallbackWhenTemplateURLServiceIsLoaded(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutomaticProfileResetterDelegateTestTemplateURLs,
       DefaultSearchProviderDataWhenNotManaged) {
  TemplateURLService* template_url_service = test_util_.model();
  test_util_.VerifyLoad();

  // Check that the "managed state" and the details returned by the delegate are
  // correct. We verify the details against the data stored by
  // TemplateURLService into Prefs.
  scoped_ptr<TemplateURL> owned_custom_dsp(CreateTestTemplateURL());
  TemplateURL* custom_dsp = owned_custom_dsp.get();
  template_url_service->Add(owned_custom_dsp.release());
  template_url_service->SetDefaultSearchProvider(custom_dsp);

  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  scoped_ptr<base::DictionaryValue> dsp_details(
      resetter_delegate()->GetDefaultSearchProviderDetails());
  scoped_ptr<base::DictionaryValue> expected_dsp_details(
      GetDefaultSearchProviderDetailsFromPrefs(prefs));

  ExpectDetailsMatch(*expected_dsp_details, *dsp_details);
  EXPECT_FALSE(resetter_delegate()->IsDefaultSearchProviderManaged());
}

TEST_F(AutomaticProfileResetterDelegateTestTemplateURLs,
       DefaultSearchProviderDataWhenManaged) {
  const char kTestSearchURL[] = "http://example.com/search?q={searchTerms}";
  const char kTestName[] = "name";
  const char kTestKeyword[] = "keyword";

  test_util_.VerifyLoad();

  EXPECT_FALSE(resetter_delegate()->IsDefaultSearchProviderManaged());

  // Set managed preferences to emulate a default search provider set by policy.
  test_util_.SetManagedDefaultSearchPreferences(
      true, kTestName, kTestKeyword, kTestSearchURL, std::string(),
      std::string(), std::string(), std::string(), std::string());

  EXPECT_TRUE(resetter_delegate()->IsDefaultSearchProviderManaged());
  scoped_ptr<base::DictionaryValue> dsp_details(
      resetter_delegate()->GetDefaultSearchProviderDetails());
  // Checking that all details are correct is already done by the above test.
  // Just make sure details are reported about the correct engine.
  base::ExpectDictStringValue(kTestSearchURL, *dsp_details, "search_url");

  // Set managed preferences to emulate that having a default search provider is
  // disabled by policy.
  test_util_.RemoveManagedDefaultSearchPreferences();
  test_util_.SetManagedDefaultSearchPreferences(
      true, std::string(), std::string(), std::string(), std::string(),
      std::string(), std::string(), std::string(), std::string());

  dsp_details = resetter_delegate()->GetDefaultSearchProviderDetails();
  EXPECT_TRUE(resetter_delegate()->IsDefaultSearchProviderManaged());
  EXPECT_TRUE(dsp_details->empty());
}

TEST_F(AutomaticProfileResetterDelegateTestTemplateURLs,
       GetPrepopulatedSearchProvidersDetails) {
  TemplateURLService* template_url_service = test_util_.model();
  test_util_.VerifyLoad();

  scoped_ptr<base::ListValue> search_engines_details(
      resetter_delegate()->GetPrepopulatedSearchProvidersDetails());

  // Do the same kind of verification as for GetDefaultSearchEngineDetails:
  // subsequently set each pre-populated engine as the default, so we can verify
  // that the details returned by the delegate about one particular engine are
  // correct in comparison to what has been stored to the Prefs.
  std::vector<TemplateURL*> prepopulated_engines =
      template_url_service->GetTemplateURLs();

  ASSERT_EQ(prepopulated_engines.size(), search_engines_details->GetSize());

  for (size_t i = 0; i < search_engines_details->GetSize(); ++i) {
    const base::DictionaryValue* details = NULL;
    ASSERT_TRUE(search_engines_details->GetDictionary(i, &details));

    std::string keyword;
    ASSERT_TRUE(details->GetString("keyword", &keyword));
    TemplateURL* search_engine =
        template_url_service->GetTemplateURLForKeyword(ASCIIToUTF16(keyword));
    ASSERT_TRUE(search_engine);
    template_url_service->SetDefaultSearchProvider(prepopulated_engines[i]);

    PrefService* prefs = profile()->GetPrefs();
    ASSERT_TRUE(prefs);
    scoped_ptr<base::DictionaryValue> expected_dsp_details(
        GetDefaultSearchProviderDetailsFromPrefs(prefs));
    ExpectDetailsMatch(*expected_dsp_details, *details);
  }
}

}  // namespace
