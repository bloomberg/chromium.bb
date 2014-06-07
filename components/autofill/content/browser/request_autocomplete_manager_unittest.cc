// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/request_autocomplete_manager.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const char kAppLocale[] = "en-US";
const AutofillManager::AutofillDownloadManagerState kDownloadState =
    AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER;

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver, AutofillClient* client)
      : AutofillManager(driver, client, kAppLocale, kDownloadState),
        autofill_enabled_(true) {}
  virtual ~TestAutofillManager() {}

  virtual bool IsAutofillEnabled() const OVERRIDE { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

 private:
  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

class CustomTestAutofillClient : public TestAutofillClient {
 public:
  CustomTestAutofillClient() : should_simulate_success_(true) {}

  virtual ~CustomTestAutofillClient() {}

  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const ResultCallback& callback) OVERRIDE {
    if (should_simulate_success_) {
      FormStructure form_structure(form);
      callback.Run(
          AutocompleteResultSuccess, base::string16(), &form_structure);
    } else {
      callback.Run(AutofillClient::AutocompleteResultErrorDisabled,
                   base::string16(),
                   NULL);
    }
  }

  void set_should_simulate_success(bool should_simulate_success) {
    should_simulate_success_ = should_simulate_success;
  }

 private:
  // Enable testing the path where a callback is called without a
  // valid FormStructure.
  bool should_simulate_success_;

  DISALLOW_COPY_AND_ASSIGN(CustomTestAutofillClient);
};

class TestContentAutofillDriver : public ContentAutofillDriver {
 public:
  TestContentAutofillDriver(content::WebContents* contents,
                            AutofillClient* client)
      : ContentAutofillDriver(contents, client, kAppLocale, kDownloadState) {
    SetAutofillManager(make_scoped_ptr<AutofillManager>(
        new TestAutofillManager(this, client)));
  }
  virtual ~TestContentAutofillDriver() {}

  TestAutofillManager* mock_autofill_manager() {
    return static_cast<TestAutofillManager*>(autofill_manager());
  }

  using ContentAutofillDriver::DidNavigateMainFrame;

  DISALLOW_COPY_AND_ASSIGN(TestContentAutofillDriver);
};

}  // namespace

class RequestAutocompleteManagerTest :
    public content::RenderViewHostTestHarness {
 public:
  RequestAutocompleteManagerTest() {}

  virtual void SetUp() OVERRIDE {
    content::RenderViewHostTestHarness::SetUp();

    driver_.reset(
        new TestContentAutofillDriver(web_contents(), &autofill_client_));
    request_autocomplete_manager_.reset(
        new RequestAutocompleteManager(driver_.get()));
  }

  virtual void TearDown() OVERRIDE {
    // Reset the driver now to cause all pref observers to be removed and avoid
    // crashes that otherwise occur in the destructor.
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Searches for an |AutofillMsg_RequestAutocompleteResult| message in the
  // queue of sent IPC messages. If none is present, returns false. Otherwise,
  // extracts the first |AutofillMsg_RequestAutocompleteResult| message, fills
  // the output parameter with the value of the message's parameter, and
  // clears the queue of sent messages.
  bool GetAutocompleteResultMessage(
      blink::WebFormElement::AutocompleteResult* result) {
    const uint32 kMsgID = AutofillMsg_RequestAutocompleteResult::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple3<blink::WebFormElement::AutocompleteResult, base::string16, FormData>
        autofill_param;
    AutofillMsg_RequestAutocompleteResult::Read(message, &autofill_param);
    *result = autofill_param.a;
    process()->sink().ClearMessages();
    return true;
  }

 protected:
  CustomTestAutofillClient autofill_client_;
  scoped_ptr<TestContentAutofillDriver> driver_;
  scoped_ptr<RequestAutocompleteManager> request_autocomplete_manager_;

  DISALLOW_COPY_AND_ASSIGN(RequestAutocompleteManagerTest);
};

TEST_F(RequestAutocompleteManagerTest, OnRequestAutocompleteSuccess) {
  blink::WebFormElement::AutocompleteResult result;
  request_autocomplete_manager_->OnRequestAutocomplete(FormData(), GURL());
  EXPECT_TRUE(GetAutocompleteResultMessage(&result));
  EXPECT_EQ(blink::WebFormElement::AutocompleteResultSuccess, result);
}

TEST_F(RequestAutocompleteManagerTest, OnRequestAutocompleteCancel) {
  blink::WebFormElement::AutocompleteResult result;
  autofill_client_.set_should_simulate_success(false);
  request_autocomplete_manager_->OnRequestAutocomplete(FormData(), GURL());
  EXPECT_TRUE(GetAutocompleteResultMessage(&result));
  EXPECT_EQ(blink::WebFormElement::AutocompleteResultErrorDisabled, result);
}

// Disabling autofill doesn't disable the dialog (it just disables the use of
// autofill in the dialog).
TEST_F(RequestAutocompleteManagerTest,
       OnRequestAutocompleteWithAutofillDisabled) {
  blink::WebFormElement::AutocompleteResult result;
  driver_->mock_autofill_manager()->set_autofill_enabled(false);
  request_autocomplete_manager_->OnRequestAutocomplete(FormData(), GURL());
  EXPECT_TRUE(GetAutocompleteResultMessage(&result));
  EXPECT_EQ(blink::WebFormElement::AutocompleteResultSuccess, result);
}

}  // namespace autofill
