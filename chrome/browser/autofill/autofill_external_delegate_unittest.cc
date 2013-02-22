// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/gfx/rect.h"

using content::BrowserThread;
using testing::_;
using WebKit::WebAutofillClient;

namespace {

// A constant value to use as the Autofill query ID.
const int kQueryId = 5;

// A constant value to use as an Autofill profile ID.
const int kAutofillProfileId = 1;

class MockAutofillExternalDelegate :
      public autofill::TestAutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate(content::WebContents* web_contents,
                               AutofillManager* autofill_manger)
      : TestAutofillExternalDelegate(web_contents, autofill_manger) {}
  ~MockAutofillExternalDelegate() {}

  MOCK_METHOD4(ApplyAutofillSuggestions, void(
      const std::vector<string16>& labels,
      const std::vector<string16>& sub_labels,
      const std::vector<string16>& icons,
      const std::vector<int>& identifiers));

  MOCK_METHOD0(ClearPreviewedForm, void());

  MOCK_METHOD1(EnsurePopupForElement, void(const gfx::RectF& element_bounds));

  MOCK_METHOD0(HideAutofillPopup, void());
};

class MockAutofillManager : public AutofillManager {
 public:
  explicit MockAutofillManager(content::WebContents* web_contents,
                               autofill::AutofillManagerDelegate* delegate)
      // Force to use the constructor designated for unit test, but we don't
      // really need personal_data in this test so we pass a NULL pointer.
      : AutofillManager(web_contents, delegate, NULL) {
  }
  virtual ~MockAutofillManager() {}

  MOCK_METHOD4(OnFillAutofillFormData,
               void(int query_id,
                    const FormData& form,
                    const FormFieldData& field,
                    int unique_id));
};

}  // namespace

class AutofillExternalDelegateUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillExternalDelegateUnitTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~AutofillExternalDelegateUnitTest() {}

 protected:
  // Set up the expectation for a platform specific OnQuery call and then
  // execute it with the given QueryId.
  void IssueOnQuery(int query_id) {
    const FormData form;
    FormFieldData field;
    field.is_focusable = true;
    field.should_autocomplete = true;
    const gfx::RectF element_bounds;

    EXPECT_CALL(*external_delegate_, EnsurePopupForElement(element_bounds));
    external_delegate_->OnQuery(query_id, form, field, element_bounds, false);
  }

  scoped_ptr<MockAutofillManager> autofill_manager_;
  scoped_ptr<testing::NiceMock<MockAutofillExternalDelegate> >
      external_delegate_;

 private:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    autofill::TabAutofillManagerDelegate::CreateForWebContents(web_contents());
    autofill_manager_.reset(new MockAutofillManager(
        web_contents(),
        autofill::TabAutofillManagerDelegate::FromWebContents(web_contents())));
    external_delegate_.reset(
        new testing::NiceMock<MockAutofillExternalDelegate>(
            web_contents(),
            autofill_manager_.get()));
  }

  virtual void TearDown() OVERRIDE {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed. Also, a real
    // AutofillManager is tied to the lifetime of the WebContents, so it must
    // be destroyed at the destruction of the WebContents.
    autofill_manager_.reset();
    external_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateUnitTest);
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery(kQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(*external_delegate_,
              ApplyAutofillSuggestions(_, _, _, testing::ElementsAre(
                  kAutofillProfileId,
                  static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                  static_cast<int>(
                      WebAutofillClient::MenuItemIDAutofillOptions))));

  // This should call ApplyAutofillSuggestions.
  std::vector<string16> autofill_item;
  autofill_item.push_back(string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(kAutofillProfileId);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);

  // Called by DidAutofillSuggestions, add expectation to remove warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  EXPECT_CALL(*external_delegate_, HideAutofillPopup());

  // This should trigger a call to hide the popup since
  // we've selected an option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0], autofill_ids[0]);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery(kQueryId);

  std::vector<string16> data_list_items;
  data_list_items.push_back(string16());
  std::vector<int> data_list_ids;
  data_list_ids.push_back(WebAutofillClient::MenuItemIDDataListEntry);

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items,
                                               data_list_items,
                                               data_list_ids);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(*external_delegate_,
              ApplyAutofillSuggestions(
                  _, _, _, testing::ElementsAre(
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDDataListEntry),
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      kAutofillProfileId,
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDAutofillOptions))));

  // This should call ApplyAutofillSuggestions.
  std::vector<string16> autofill_item;
  autofill_item.push_back(string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(kAutofillProfileId);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  // The enum must be cast to an int to prevent compile errors on linux_rel.
  EXPECT_CALL(*external_delegate_,
              ApplyAutofillSuggestions(
                  _, _, _, testing::ElementsAre(
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDDataListEntry))));

  autofill_item = std::vector<string16>();
  autofill_ids = std::vector<int>();
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _)).Times(0);
  EXPECT_CALL(*external_delegate_, ClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(-1);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _)).Times(0);
  external_delegate_->DidAcceptSuggestion(string16(), -1);
}

// Test that the ClearPreview IPC is only sent the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Called by DidSelectSuggestion, add expectation to remove
  // warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  EXPECT_CALL(*external_delegate_, ClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(
      WebAutofillClient::MenuItemIDPasswordEntry);

  EXPECT_CALL(*external_delegate_, ClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(1);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(*external_delegate_, EnsurePopupForElement(_));
  EXPECT_CALL(*external_delegate_, ApplyAutofillSuggestions(_, _, _, _));

  autofill::GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_CALL(*external_delegate_, HideAutofillPopup());

  external_delegate_->DidEndTextFieldEditing();
}

// Test that the popup is marked as visible after recieving password
// suggestions.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegatePasswordSuggestions) {
  std::vector<string16> suggestions;
  suggestions.push_back(string16());

  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  const gfx::RectF element_bounds;

  EXPECT_CALL(*external_delegate_, EnsurePopupForElement(element_bounds));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(*external_delegate_,
              ApplyAutofillSuggestions(_, _, _, testing::ElementsAre(
                  static_cast<int>(
                      WebAutofillClient::MenuItemIDPasswordEntry))));

  external_delegate_->OnShowPasswordSuggestions(suggestions,
                                                field,
                                                element_bounds);

  // Called by DidAutofillSuggestions, add expectation to remove warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  EXPECT_CALL(*external_delegate_, HideAutofillPopup());

  // This should trigger a call to hide the popup since
  // we've selected an option.
  external_delegate_->DidAcceptSuggestion(
      suggestions[0],
      WebAutofillClient::MenuItemIDPasswordEntry);
}
