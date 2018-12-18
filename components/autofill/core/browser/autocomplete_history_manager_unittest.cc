// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/webdata_services/web_data_service_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::Eq;
using testing::Field;
using testing::Return;
using testing::UnorderedElementsAre;

namespace autofill {

namespace {

class MockWebDataService : public AutofillWebDataService {
 public:
  MockWebDataService()
      : AutofillWebDataService(base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get()) {}

  MOCK_METHOD1(AddFormFields, void(const std::vector<FormFieldData>&));
  MOCK_METHOD1(CancelRequest, void(int));
  MOCK_METHOD4(GetFormValuesForElementName,
               WebDataServiceBase::Handle(const base::string16& name,
                                          const base::string16& prefix,
                                          int limit,
                                          WebDataServiceConsumer* consumer));

 protected:
  ~MockWebDataService() override {}
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() : prefs_(test::PrefServiceForTesting()) {}
  ~MockAutofillClient() override {}
  PrefService* GetPrefs() override { return prefs_.get(); }

 private:
  std::unique_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class MockSuggestionsHandler
    : public AutocompleteHistoryManager::SuggestionsHandler {
 public:
  MockSuggestionsHandler() : weak_ptr_factory_(this) {}

  MOCK_METHOD2(OnSuggestionsReturned,
               void(int query_id, const std::vector<Suggestion>& suggestions));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockSuggestionsHandler);
};

}  // namespace

class AutocompleteHistoryManagerTest : public testing::Test {
 protected:
  AutocompleteHistoryManagerTest() {}

  void SetUp() override {
    web_data_service_ = base::MakeRefCounted<MockWebDataService>();
    autofill_client_ = std::make_unique<MockAutofillClient>();
    autocomplete_manager_ = std::make_unique<AutocompleteHistoryManager>();
    autocomplete_manager_->Init(web_data_service_, false);
  }

  void TearDown() override {
    // Ensure there are no left-over entries in the map (leak check).
    EXPECT_TRUE(PendingQueriesEmpty());

    autocomplete_manager_.reset();
  }

  bool PendingQueriesEmpty() {
    return !autocomplete_manager_ ||
           autocomplete_manager_->pending_queries_.empty();
  }

  static bool IsEmptySuggestionVector(
      const std::vector<Suggestion>& suggestions) {
    return suggestions.empty();
  }

  static bool NonEmptySuggestionVector(
      const std::vector<Suggestion>& suggestions) {
    return !suggestions.empty();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<MockWebDataService> web_data_service_;
  std::unique_ptr<AutocompleteHistoryManager> autocomplete_manager_;
  std::unique_ptr<MockAutofillClient> autofill_client_;
};

// Tests that credit card numbers are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, CreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Valid Visa credit card number pulled from the paypal help site.
  FormFieldData valid_cc;
  valid_cc.label = ASCIIToUTF16("Credit Card");
  valid_cc.name = ASCIIToUTF16("ccnum");
  valid_cc.value = ASCIIToUTF16("4012888888881881");
  valid_cc.form_control_type = "text";
  form.fields.push_back(valid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Contrary test to AutocompleteHistoryManagerTest.CreditCardNumberValue.  The
// value being submitted is not a valid credit card number, so it will be sent
// to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonCreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Invalid credit card number.
  FormFieldData invalid_cc;
  invalid_cc.label = ASCIIToUTF16("Credit Card");
  invalid_cc.name = ASCIIToUTF16("ccnum");
  invalid_cc.value = ASCIIToUTF16("4580123456789012");
  invalid_cc.form_control_type = "text";
  form.fields.push_back(invalid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Tests that SSNs are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, SSNValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData ssn;
  ssn.label = ASCIIToUTF16("Social Security Number");
  ssn.name = ASCIIToUTF16("ssn");
  ssn.value = ASCIIToUTF16("078-05-1120");
  ssn.form_control_type = "text";
  form.fields.push_back(ssn);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Verify that autocomplete text is saved for search fields.
TEST_F(AutocompleteHistoryManagerTest, SearchField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

TEST_F(AutocompleteHistoryManagerTest, AutocompleteFeatureOff) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/false);
}

// Tests that text entered into fields specifying autocomplete="off" is not sent
// to the WebDatabase to be saved. Note this is also important as the mechanism
// for preventing CVCs from being saved.
// See AutofillManagerTest.DontSaveCvcInAutocompleteHistory
TEST_F(AutocompleteHistoryManagerTest, FieldWithAutocompleteOff) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Field specifying autocomplete="off".
  FormFieldData field;
  field.label = ASCIIToUTF16("Something esoteric");
  field.name = ASCIIToUTF16("esoterica");
  field.value = ASCIIToUTF16("a truly esoteric value, I assure you");
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Shouldn't save entries when in Incognito mode.
TEST_F(AutocompleteHistoryManagerTest, Incognito) {
  autocomplete_manager_->Init(web_data_service_, /*is_off_the_record_=*/true);
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Tests that text entered into fields that are not focusable is not sent to the
// WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonFocusableField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Unfocusable field.
  FormFieldData field;
  field.label = ASCIIToUTF16("Something esoteric");
  field.name = ASCIIToUTF16("esoterica");
  field.value = ASCIIToUTF16("a truly esoteric value, I assure you");
  field.form_control_type = "text";
  field.is_focusable = false;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Tests that text entered into presentation fields is not sent to the
// WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, PresentationField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Presentation field.
  FormFieldData field;
  field.label = ASCIIToUTF16("Something esoteric");
  field.name = ASCIIToUTF16("esoterica");
  field.value = ASCIIToUTF16("a truly esoteric value, I assure you");
  field.form_control_type = "text";
  field.role = FormFieldData::ROLE_ATTRIBUTE_PRESENTATION;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form,
                                          /*is_autocomplete_enabled=*/true);
}

// Make sure our handler is called at the right time.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_Empty) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  std::vector<base::string16> expected_values;

  std::unique_ptr<WDTypedResult> mocked_results =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id, /*is_autocomplete_enabled=*/true, test_name, test_prefix,
      "Some Type", suggestions_handler->GetWeakPtr());

  // Setting up mock to verify that DB response triggers a call to the handler's
  // OnSuggestionsReturned
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(test_query_id,
                                    testing::Truly(IsEmptySuggestionVector)));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  std::vector<base::string16> expected_values = {ASCIIToUTF16("SomePrefixOne")};

  std::unique_ptr<WDTypedResult> mocked_results =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id, /*is_autocomplete_enabled=*/true, test_name, test_prefix,
      "Some Type", suggestions_handler->GetWeakPtr());

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_query_id, UnorderedElementsAre(Field(
                                     &Suggestion::value, expected_values[0]))));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

// Tests that we don't return any suggestion if we only have one suggestion that
// is case-sensitive equal to the given prefix.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue_EqualsPrefix) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  std::vector<base::string16> expected_values = {test_prefix};

  std::unique_ptr<WDTypedResult> mocked_results =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id, /*is_autocomplete_enabled=*/true, test_name, test_prefix,
      "Some Type", suggestions_handler->GetWeakPtr());

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(test_query_id,
                                    testing::Truly(IsEmptySuggestionVector)));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

// Tests the case sensitivity of the unique suggestion equal to the prefix
// filter.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue_EqualsPrefix_DiffCase) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  std::vector<base::string16> expected_values = {ASCIIToUTF16("someprefix")};

  std::unique_ptr<WDTypedResult> mocked_results =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id, /*is_autocomplete_enabled=*/true, test_name, test_prefix,
      "Some Type", suggestions_handler->GetWeakPtr());

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_query_id, UnorderedElementsAre(Field(
                                     &Suggestion::value, expected_values[0]))));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_TwoRequests_OneHandler_Cancels) {
  int mocked_db_query_id_first = 100;
  int mocked_db_query_id_second = 101;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id_first = 2;
  int test_query_id_second = 3;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  std::vector<base::string16> expected_values_first = {
      ASCIIToUTF16("SomePrefixOne")};

  std::vector<base::string16> expected_values_second = {
      ASCIIToUTF16("SomePrefixTwo")};

  std::unique_ptr<WDTypedResult> mocked_results_first =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values_first);

  std::unique_ptr<WDTypedResult> mocked_results_second =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values_second);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_first))
      .WillOnce(Return(mocked_db_query_id_second));

  // Simulate request for the first suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_first, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler->GetWeakPtr());

  // Simulate request for the second suggestions (this will cancel the first
  // one).
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_first))
      .Times(1);
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_second, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler->GetWeakPtr());

  // Setting up mock to verify that we can get the second response first.
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_query_id_second,
                  UnorderedElementsAre(
                      Field(&Suggestion::value, expected_values_second[0]))));

  // Simulate response from DB, second request comes back before.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_second, std::move(mocked_results_second));

  // Setting up mock to verify that the handler doesn't get called for the first
  // request, which was cancelled.
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(test_query_id_first, _))
      .Times(0);

  // Simulate response from DB, first request comes back after.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_first, std::move(mocked_results_first));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_TwoRequests_TwoHandlers) {
  int mocked_db_query_id_first = 100;
  int mocked_db_query_id_second = 101;

  auto suggestions_handler_first = std::make_unique<MockSuggestionsHandler>();
  auto suggestions_handler_second = std::make_unique<MockSuggestionsHandler>();
  int test_query_id_first = 2;
  int test_query_id_second = 3;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  std::vector<base::string16> expected_values_first = {
      ASCIIToUTF16("SomePrefixOne")};

  std::vector<base::string16> expected_values_second = {
      ASCIIToUTF16("SomePrefixTwo")};

  std::unique_ptr<WDTypedResult> mocked_results_first =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values_first);

  std::unique_ptr<WDTypedResult> mocked_results_second =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values_second);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_first))
      .WillOnce(Return(mocked_db_query_id_second));

  // Simulate request for the first suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_first, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler_first->GetWeakPtr());

  // Simulate request for the second suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_second, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler_second->GetWeakPtr());

  // Setting up mock to verify that we get the second response first.
  EXPECT_CALL(*suggestions_handler_second.get(),
              OnSuggestionsReturned(
                  test_query_id_second,
                  UnorderedElementsAre(
                      Field(&Suggestion::value, expected_values_second[0]))));

  // Simulate response from DB, second request comes back before.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_second, std::move(mocked_results_second));

  // Setting up mock to verify that we get the first response second.
  EXPECT_CALL(*suggestions_handler_first.get(),
              OnSuggestionsReturned(
                  test_query_id_first,
                  UnorderedElementsAre(
                      Field(&Suggestion::value, expected_values_first[0]))));

  // Simulate response from DB, first request comes back after.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_first, std::move(mocked_results_first));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_CancelOne_ReturnOne) {
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  // Initialize variables for the first handler, which is the one that will be
  // cancelled.
  auto suggestions_handler_one = std::make_unique<MockSuggestionsHandler>();
  int mocked_db_query_id_one = 100;
  int test_query_id_one = 1;
  std::vector<base::string16> expected_values_one = {
      ASCIIToUTF16("SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results_one =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values_one);

  // Initialize variables for the second handler, which will be fulfilled.
  auto suggestions_handler_two = std::make_unique<MockSuggestionsHandler>();
  int test_query_id_two = 2;
  int mocked_db_query_id_two = 101;
  std::vector<base::string16> expected_values_two = {
      ASCIIToUTF16("SomePrefixTwo")};
  std::unique_ptr<WDTypedResult> mocked_results_two =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, expected_values_two);

  // Simulate first handler request for autocomplete suggestions.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_one))
      .WillOnce(Return(mocked_db_query_id_two));

  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_one, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler_one->GetWeakPtr());

  // Simlate second handler request for autocomplete suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_two, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler_two->GetWeakPtr());

  // Simlate first handler cancelling its request.
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_one))
      .Times(1);
  autocomplete_manager_->CancelPendingQueries(suggestions_handler_one.get());

  // Simulate second handler receiving the suggestions.
  EXPECT_CALL(
      *suggestions_handler_two.get(),
      OnSuggestionsReturned(test_query_id_two,
                            UnorderedElementsAre(Field(
                                &Suggestion::value, expected_values_two[0]))));
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_two, std::move(mocked_results_two));

  // Make sure first handler is not called when the DB responds.
  EXPECT_CALL(*suggestions_handler_one.get(),
              OnSuggestionsReturned(test_query_id_one, _))
      .Times(0);
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_one, std::move(mocked_results_one));
}

// // Verify that no autocomplete suggestion is returned for textarea and UMA is
// // logged correctly.
TEST_F(AutocompleteHistoryManagerTest, NoAutocompleteSuggestionsForTextarea) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Address", "address", "", "textarea", &field);

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(0, testing::Truly(IsEmptySuggestionVector)));

  base::HistogramTester histogram_tester;

  autocomplete_manager_->OnGetAutocompleteSuggestions(
      0, /*is_autocomplete_enabled=*/true, field.name, field.value,
      field.form_control_type, suggestions_handler->GetWeakPtr());

  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 1, 0);
}

// // Verify that autocomplete suggestion is returned and suggestions is logged
// // correctly.
TEST_F(AutocompleteHistoryManagerTest, AutocompleteUMAQueryCreated) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  FormFieldData field;
  test::CreateTestFormField("Address", "address", "", "text", &field);

  // Mock returned handle to match it in OnWebDataServiceRequestDone().
  WebDataServiceBase::Handle mock_handle = 1;

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(field.name, field.value, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mock_handle));

  // Verify that the query has been created.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(0, testing::Truly(IsEmptySuggestionVector)));
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      0, /*is_autocomplete_enabled=*/true, field.name, field.value,
      field.form_control_type, suggestions_handler->GetWeakPtr());
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 1, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 0, 0);

  // Mock no suggestion returned and verify that the suggestion UMA is correct.
  std::unique_ptr<WDTypedResult> result =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, std::vector<base::string16>());
  autocomplete_manager_->OnWebDataServiceRequestDone(mock_handle,
                                                     std::move(result));

  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 1, 0);

  // Changed the returned handle
  // Changed field's name to trigger UMA again.
  mock_handle = 2;
  test::CreateTestFormField("Address", "address1", "", "text", &field);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(field.name, field.value, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mock_handle));

  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(0, testing::Truly(NonEmptySuggestionVector)));
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      0, /*is_autocomplete_enabled=*/true, field.name, field.value,
      field.form_control_type, suggestions_handler->GetWeakPtr());
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 1, 2);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 0, 0);

  // Mock one suggestion returned and verify that the suggestion UMA is correct.
  std::vector<base::string16> values;
  values.push_back(ASCIIToUTF16("value"));
  result = std::make_unique<WDResult<std::vector<base::string16>>>(
      AUTOFILL_VALUE_RESULT, values);
  autocomplete_manager_->OnWebDataServiceRequestDone(mock_handle,
                                                     std::move(result));

  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 1, 1);
}

TEST_F(AutocompleteHistoryManagerTest, DestructorCancelsRequests) {
  int mocked_db_query_id_first = 100;
  int mocked_db_query_id_second = 101;

  auto suggestions_handler_first = std::make_unique<MockSuggestionsHandler>();
  auto suggestions_handler_second = std::make_unique<MockSuggestionsHandler>();
  int test_query_id_first = 2;
  int test_query_id_second = 3;
  auto test_name = ASCIIToUTF16("Some Field Name");
  auto test_prefix = ASCIIToUTF16("SomePrefix");

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_name, test_prefix, _,
                                          autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_first))
      .WillOnce(Return(mocked_db_query_id_second));

  // Simulate request for the first suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_first, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler_first->GetWeakPtr());

  // Simulate request for the second suggestions.
  autocomplete_manager_->OnGetAutocompleteSuggestions(
      test_query_id_second, /*is_autocomplete_enabled=*/true, test_name,
      test_prefix, "Some Type", suggestions_handler_second->GetWeakPtr());

  // Expect cancel calls for both requests.
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_first))
      .Times(1);
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_second))
      .Times(1);

  autocomplete_manager_.reset();

  EXPECT_TRUE(PendingQueriesEmpty());
}

}  // namespace autofill
