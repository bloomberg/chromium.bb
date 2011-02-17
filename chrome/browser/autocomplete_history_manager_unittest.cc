// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
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

class AutocompleteHistoryManagerTest : public RenderViewHostTestHarness {
 protected:
  AutocompleteHistoryManagerTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
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
  webkit_glue::FormField valid_cc(ASCIIToUTF16("Credit Card"),
                                  ASCIIToUTF16("ccnum"),
                                  ASCIIToUTF16("4012888888881881"),
                                  ASCIIToUTF16("text"),
                                  20,
                                  false);
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
  webkit_glue::FormField invalid_cc(ASCIIToUTF16("Credit Card"),
                                    ASCIIToUTF16("ccnum"),
                                    ASCIIToUTF16("4580123456789012"),
                                    ASCIIToUTF16("text"),
                                    20,
                                    false);
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

  webkit_glue::FormField ssn(ASCIIToUTF16("Social Security Number"),
                             ASCIIToUTF16("ssn"),
                             ASCIIToUTF16("078-05-1120"),
                             ASCIIToUTF16("text"),
                             20,
                             false);
  form.fields.push_back(ssn);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnFormSubmitted(form);
}
