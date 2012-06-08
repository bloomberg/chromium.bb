// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/form_data.h"
#include "webkit/forms/form_field.h"

using content::BrowserThread;
using testing::_;
using webkit::forms::FormData;
using webkit::forms::FormField;
using WebKit::WebAutofillClient;

namespace {

// A constant value to use as the Autofill query ID.
const int kQueryId = 5;

// A constant value to use as an Autofill profile ID.
const int kAutofillProfileId = 1;

class MockAutofillExternalDelegate : public TestAutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate(TabContents* tab_contents,
                               AutofillManager* autofill_manger)
      : TestAutofillExternalDelegate(tab_contents, autofill_manger) {}
  ~MockAutofillExternalDelegate() {}

  MOCK_METHOD4(ApplyAutofillSuggestions, void(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids));

  MOCK_METHOD4(OnQueryPlatformSpecific,
               void(int query_id,
                    const webkit::forms::FormData& form,
                    const webkit::forms::FormField& field,
                    const gfx::Rect& bounds));

  MOCK_METHOD0(ClearPreviewedForm, void());

  MOCK_METHOD0(HideAutofillPopup, void());

 private:
  virtual void HideAutofillPopupInternal() {};
};

class MockAutofillManager : public AutofillManager {
 public:
  explicit MockAutofillManager(TabContents* tab_contents)
      // Force to use the constructor designated for unit test, but we don't
      // really need personal_data in this test so we pass a NULL pointer.
      : AutofillManager(tab_contents, NULL) {}

  MOCK_METHOD4(OnFillAutofillFormData,
               void(int query_id,
                    const webkit::forms::FormData& form,
                    const webkit::forms::FormField& field,
                    int unique_id));

 protected:
  virtual ~MockAutofillManager() {}
};

}  // namespace

class AutofillExternalDelegateUnitTest : public TabContentsTestHarness {
 public:
  AutofillExternalDelegateUnitTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~AutofillExternalDelegateUnitTest() {}

  virtual void SetUp() OVERRIDE {
    TabContentsTestHarness::SetUp();
    autofill_manager_ = new MockAutofillManager(tab_contents());
    external_delegate_.reset(new MockAutofillExternalDelegate(
        tab_contents(),
        autofill_manager_));
  }

 protected:
  // Set up the expectation for a platform specific OnQuery call and then
  // execute it with the given QueryId.
  void IssueOnQuery(int query_id) {
    const FormData form;
    FormField field;
    field.is_focusable = true;
    field.should_autocomplete = true;
    const gfx::Rect bounds;

    EXPECT_CALL(*external_delegate_,
                OnQueryPlatformSpecific(query_id, form, field, bounds));

    // This should call OnQueryPlatform specific.
    external_delegate_->OnQuery(query_id, form, field, bounds, false);
  }

  scoped_refptr<MockAutofillManager> autofill_manager_;
  scoped_ptr<MockAutofillExternalDelegate> external_delegate_;

 private:
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


  EXPECT_CALL(*external_delegate_, HideAutofillPopup());

  // Called by DidAutofillSuggestions, add expectation to remove warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  // This should trigger a call to hide the popup since
  // we've selected an option.
  external_delegate_->DidAcceptAutofillSuggestions(autofill_item[0],
                                                   autofill_ids[0], 0);
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
  external_delegate_->SelectAutofillSuggestionAtIndex(-1);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _)).Times(0);
  external_delegate_->DidAcceptAutofillSuggestions(string16(), -1, 0);
}

// Test that the ClearPreview IPC is only sent the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Called by SelectAutofillSuggestionAtIndex, add expectation to remove
  // warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  EXPECT_CALL(*external_delegate_, ClearPreviewedForm()).Times(1);
  external_delegate_->SelectAutofillSuggestionAtIndex(
      WebAutofillClient::MenuItemIDPasswordEntry);

  EXPECT_CALL(*external_delegate_, ClearPreviewedForm()).Times(1);
  external_delegate_->SelectAutofillSuggestionAtIndex(1);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(*external_delegate_, HideAutofillPopup());

  external_delegate_->DidEndTextFieldEditing();
}
