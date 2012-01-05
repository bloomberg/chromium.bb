// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/form_data.h"
#include "webkit/forms/form_field.h"

using content::BrowserThread;
using testing::_;
using webkit::forms::FormData;
using webkit::forms::FormField;

namespace {

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit MockAutofillExternalDelegate(TabContentsWrapper* wrapper,
                                        AutofillManager* autofill_manager)
      : AutofillExternalDelegate(wrapper, autofill_manager) {}
  virtual ~MockAutofillExternalDelegate() {}

  virtual void HideAutofillPopup() OVERRIDE {}

  MOCK_METHOD5(ApplyAutofillSuggestions, void(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids,
      int separator_index));

  MOCK_METHOD4(OnQueryPlatformSpecific,
               void(int query_id,
                    const webkit::forms::FormData& form,
                    const webkit::forms::FormField& field,
                    const gfx::Rect& bounds));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillExternalDelegate);
};

}  // namespace

class AutofillExternalDelegateTest : public TabContentsWrapperTestHarness {
 public:
  AutofillExternalDelegateTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~AutofillExternalDelegateTest() {}

  virtual void SetUp() OVERRIDE {
    TabContentsWrapperTestHarness::SetUp();
    autofill_manager_ = new AutofillManager(contents_wrapper());
  }

 protected:
  scoped_refptr<AutofillManager> autofill_manager_;

 private:
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateTest);
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateTest, TestExternalDelegateVirtualCalls) {
  MockAutofillExternalDelegate external_delegate(contents_wrapper(),
                                                 autofill_manager_);
  const int kQueryId = 5;
  const FormData form;
  FormField field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  const gfx::Rect bounds;

  EXPECT_CALL(external_delegate,
              OnQueryPlatformSpecific(kQueryId, form, field, bounds));

  // This should call OnQueryPlatform specific.
  external_delegate.OnQuery(kQueryId, form, field, bounds, false);


  EXPECT_CALL(external_delegate, ApplyAutofillSuggestions(_, _, _, _, _));

  // This should call ApplyAutofillSuggestions.
  std::vector<string16> autofill_item;
  autofill_item.push_back(string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(1);
  external_delegate.OnSuggestionsReturned(kQueryId,
                                          autofill_item,
                                          autofill_item,
                                          autofill_item,
                                          autofill_ids);
}
