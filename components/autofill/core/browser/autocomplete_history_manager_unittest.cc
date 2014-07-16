// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_data.h"
#include "components/webdata/common/web_data_service_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

using base::ASCIIToUTF16;
using testing::_;

namespace autofill {

namespace {

class MockWebDataService : public AutofillWebDataService {
 public:
  MockWebDataService()
      : AutofillWebDataService(base::MessageLoopProxy::current(),
                               base::MessageLoopProxy::current()) {}

  MOCK_METHOD1(AddFormFields, void(const std::vector<FormFieldData>&));

 protected:
  virtual ~MockWebDataService() {}
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient(scoped_refptr<MockWebDataService> web_data_service)
      : web_data_service_(web_data_service),
        prefs_(test::PrefServiceForTesting()) {}
  virtual ~MockAutofillClient() {}
  virtual scoped_refptr<AutofillWebDataService>
      GetDatabase() OVERRIDE { return web_data_service_; }
  virtual PrefService* GetPrefs() OVERRIDE { return prefs_.get(); }

 private:
  scoped_refptr<MockWebDataService> web_data_service_;
  scoped_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

}  // namespace

class AutocompleteHistoryManagerTest : public testing::Test {
 protected:
  AutocompleteHistoryManagerTest() {}

  virtual void SetUp() OVERRIDE {
    web_data_service_ = new MockWebDataService();
    autofill_client_.reset(new MockAutofillClient(web_data_service_));
    autofill_driver_.reset(new TestAutofillDriver());
    autocomplete_manager_.reset(new AutocompleteHistoryManager(
        autofill_driver_.get(), autofill_client_.get()));
  }

  virtual void TearDown() OVERRIDE {
    autocomplete_manager_.reset();
  }

  base::MessageLoop message_loop_;
  scoped_refptr<MockWebDataService> web_data_service_;
  scoped_ptr<AutocompleteHistoryManager> autocomplete_manager_;
  scoped_ptr<AutofillDriver> autofill_driver_;
  scoped_ptr<MockAutofillClient> autofill_client_;
};

// Tests that credit card numbers are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, CreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  // Valid Visa credit card number pulled from the paypal help site.
  FormFieldData valid_cc;
  valid_cc.label = ASCIIToUTF16("Credit Card");
  valid_cc.name = ASCIIToUTF16("ccnum");
  valid_cc.value = ASCIIToUTF16("4012888888881881");
  valid_cc.form_control_type = "text";
  form.fields.push_back(valid_cc);

  EXPECT_CALL(*web_data_service_.get(), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnFormSubmitted(form);
}

// Contrary test to AutocompleteHistoryManagerTest.CreditCardNumberValue.  The
// value being submitted is not a valid credit card number, so it will be sent
// to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonCreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  // Invalid credit card number.
  FormFieldData invalid_cc;
  invalid_cc.label = ASCIIToUTF16("Credit Card");
  invalid_cc.name = ASCIIToUTF16("ccnum");
  invalid_cc.value = ASCIIToUTF16("4580123456789012");
  invalid_cc.form_control_type = "text";
  form.fields.push_back(invalid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnFormSubmitted(form);
}

// Tests that SSNs are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, SSNValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  FormFieldData ssn;
  ssn.label = ASCIIToUTF16("Social Security Number");
  ssn.name = ASCIIToUTF16("ssn");
  ssn.value = ASCIIToUTF16("078-05-1120");
  ssn.form_control_type = "text";
  form.fields.push_back(ssn);

  EXPECT_CALL(*web_data_service_.get(), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnFormSubmitted(form);
}

// Verify that autocomplete text is saved for search fields.
TEST_F(AutocompleteHistoryManagerTest, SearchField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  // Search field.
  FormFieldData search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnFormSubmitted(form);
}

namespace {

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate(AutofillManager* autofill_manager,
                               AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver) {}
  virtual ~MockAutofillExternalDelegate() {}

  MOCK_METHOD5(OnSuggestionsReturned,
               void(int query_id,
                    const std::vector<base::string16>& autofill_values,
                    const std::vector<base::string16>& autofill_labels,
                    const std::vector<base::string16>& autofill_icons,
                    const std::vector<int>& autofill_unique_ids));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillExternalDelegate);
};

class TestAutocompleteHistoryManager : public AutocompleteHistoryManager {
 public:
  TestAutocompleteHistoryManager(AutofillDriver* driver, AutofillClient* client)
      : AutocompleteHistoryManager(driver, client) {}

  using AutocompleteHistoryManager::SendSuggestions;
};

}  // namespace

// Make sure our external delegate is called at the right time.
TEST_F(AutocompleteHistoryManagerTest, ExternalDelegate) {
  TestAutocompleteHistoryManager autocomplete_history_manager(
      autofill_driver_.get(), autofill_client_.get());

  scoped_ptr<AutofillManager> autofill_manager(
      new AutofillManager(autofill_driver_.get(),
                          autofill_client_.get(),
                          "en-US",
                          AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER));

  MockAutofillExternalDelegate external_delegate(autofill_manager.get(),
                                                 autofill_driver_.get());
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  // Should trigger a call to OnSuggestionsReturned, verified by the mock.
  EXPECT_CALL(external_delegate, OnSuggestionsReturned(_, _, _, _, _));
  autocomplete_history_manager.SendSuggestions(NULL);
}

// Verify that no autocomplete suggestion is returned for textarea.
TEST_F(AutocompleteHistoryManagerTest, NoAutocompleteSuggestionsForTextarea) {
  TestAutocompleteHistoryManager autocomplete_history_manager(
      autofill_driver_.get(), autofill_client_.get());

  scoped_ptr<AutofillManager> autofill_manager(
      new AutofillManager(autofill_driver_.get(),
                          autofill_client_.get(),
                          "en-US",
                          AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER));

  MockAutofillExternalDelegate external_delegate(autofill_manager.get(),
                                                 autofill_driver_.get());
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("Address", "address", "", "textarea", &field);

  EXPECT_CALL(external_delegate,
              OnSuggestionsReturned(0,
                                    std::vector<base::string16>(),
                                    std::vector<base::string16>(),
                                    std::vector<base::string16>(),
                                    std::vector<int>()));
  autocomplete_history_manager.OnGetAutocompleteSuggestions(
      0,
      field.name,
      field.value,
      field.form_control_type,
      std::vector<base::string16>(),
      std::vector<base::string16>(),
      std::vector<base::string16>(),
      std::vector<int>());
}

}  // namespace autofill
