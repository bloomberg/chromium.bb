// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autocomplete_history_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/webdata/autofill_web_data_service_impl.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/form_data.h"

using content::BrowserThread;
using content::WebContents;
using testing::_;
using webkit::forms::FormData;

class MockWebDataService : public WebDataService {
 public:
  MOCK_METHOD1(AddFormFields,
               void(const std::vector<webkit::forms::FormField>&));  // NOLINT

 protected:
  virtual ~MockWebDataService() {}
};

class AutocompleteHistoryManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AutocompleteHistoryManagerTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()),
        db_thread_(BrowserThread::DB) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    web_data_service_ = new MockWebDataService();
    autocomplete_manager_.reset(
        new AutocompleteHistoryManager(
            contents(),
            &profile_,
            scoped_ptr<AutofillWebDataService>(
                new AutofillWebDataServiceImpl(web_data_service_))));
  }

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  TestingProfile profile_;
  scoped_refptr<MockWebDataService> web_data_service_;
  scoped_ptr<AutocompleteHistoryManager> autocomplete_manager_;
};

// Tests that credit card numbers are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, CreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  // Valid Visa credit card number pulled from the paypal help site.
  webkit::forms::FormField valid_cc;
  valid_cc.label = ASCIIToUTF16("Credit Card");
  valid_cc.name = ASCIIToUTF16("ccnum");
  valid_cc.value = ASCIIToUTF16("4012888888881881");
  valid_cc.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(valid_cc);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnFormSubmitted(form);
}

// Contrary test to AutocompleteHistoryManagerTest.CreditCardNumberValue.  The
// value being submitted is not a valid credit card number, so it will be sent
// to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonCreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  // Invalid credit card number.
  webkit::forms::FormField invalid_cc;
  invalid_cc.label = ASCIIToUTF16("Credit Card");
  invalid_cc.name = ASCIIToUTF16("ccnum");
  invalid_cc.value = ASCIIToUTF16("4580123456789012");
  invalid_cc.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(invalid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnFormSubmitted(form);
}

// Tests that SSNs are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, SSNValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  webkit::forms::FormField ssn;
  ssn.label = ASCIIToUTF16("Social Security Number");
  ssn.name = ASCIIToUTF16("ssn");
  ssn.value = ASCIIToUTF16("078-05-1120");
  ssn.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(ssn);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnFormSubmitted(form);
}

// Verify that autocomplete text is saved for search fields.
TEST_F(AutocompleteHistoryManagerTest, SearchField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  // Search field.
  webkit::forms::FormField search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = ASCIIToUTF16("search");
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnFormSubmitted(form);
}

namespace {

class MockAutofillExternalDelegate : public TestAutofillExternalDelegate {
 public:
  explicit MockAutofillExternalDelegate(TabContents* tab_contents)
      : TestAutofillExternalDelegate(tab_contents, NULL) {}
  virtual ~MockAutofillExternalDelegate() {}

  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE {};

  MOCK_METHOD5(OnSuggestionsReturned,
               void(int query_id,
                    const std::vector<string16>& autofill_values,
                    const std::vector<string16>& autofill_labels,
                    const std::vector<string16>& autofill_icons,
                    const std::vector<int>& autofill_unique_ids));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillExternalDelegate);
};

class AutocompleteHistoryManagerStubSend : public AutocompleteHistoryManager {
 public:
  explicit AutocompleteHistoryManagerStubSend(
      WebContents* web_contents,
      Profile* profile,
      scoped_ptr<AutofillWebDataService> wds)
      : AutocompleteHistoryManager(web_contents, profile, wds.Pass()) {}

  // Intentionally swallow the message.
  virtual bool Send(IPC::Message* message) { delete message; return true; }
};

}  // namespace

// Make sure our external delegate is called at the right time.
TEST_F(AutocompleteHistoryManagerTest, ExternalDelegate) {
  // Local version with a stubbed out Send()
  AutocompleteHistoryManagerStubSend autocomplete_history_manager(
      contents(),
      &profile_,
      scoped_ptr<AutofillWebDataService>(
          new AutofillWebDataServiceImpl(web_data_service_)));

  MockAutofillExternalDelegate external_delegate(
      TabContents::FromWebContents(contents()));
  EXPECT_CALL(external_delegate, OnSuggestionsReturned(_, _,  _,  _,  _));
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  // Should trigger a call to OnSuggestionsReturned, verified by the mock.
  autocomplete_history_manager.SendSuggestions(NULL);
}
