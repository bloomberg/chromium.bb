// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_data.h"

using testing::_;
using webkit_glue::FormData;

class MockWebDataService : public WebDataService {
 public:
  MOCK_METHOD1(AddFormFields,
               void(const std::vector<webkit_glue::FormField>&));  // NOLINT
};

class AutocompleteHistoryManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AutocompleteHistoryManagerTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    web_data_service_ = new MockWebDataService();
    autocomplete_manager_.reset(new AutocompleteHistoryManager(
        contents(), &profile_, web_data_service_));
  }

  BrowserThread ui_thread_;

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
  webkit_glue::FormField valid_cc;
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
  webkit_glue::FormField invalid_cc;
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

  webkit_glue::FormField ssn;
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
  webkit_glue::FormField search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = ASCIIToUTF16("search");
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnFormSubmitted(form);
}
