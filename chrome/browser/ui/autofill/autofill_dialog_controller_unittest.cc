// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestAutofillDialogView : public AutofillDialogView {
 public:
  TestAutofillDialogView() {}
  virtual ~TestAutofillDialogView() {}

  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void UpdateNotificationArea() OVERRIDE {}
  virtual void UpdateAccountChooser() OVERRIDE {}
  virtual void UpdateButtonStrip() OVERRIDE {}
  virtual void UpdateSection(DialogSection section) OVERRIDE {}
  virtual void GetUserInput(DialogSection section, DetailOutputMap* output)
      OVERRIDE {}
  virtual string16 GetCvc() OVERRIDE { return string16(); }
  virtual bool UseBillingForShipping() OVERRIDE { return false; }
  virtual bool SaveDetailsInWallet() OVERRIDE { return false; }
  virtual bool SaveDetailsLocally() OVERRIDE { return false; }
  virtual const content::NavigationController* ShowSignIn() OVERRIDE {
    return NULL;
  }
  virtual void HideSignIn() OVERRIDE {}
  virtual void UpdateProgressBar(double value) OVERRIDE {}
  virtual void ModelChanged() OVERRIDE {}
  virtual void SubmitForTesting() OVERRIDE {}
  virtual void CancelForTesting() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogView);
};

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const AutofillMetrics& metric_logger,
      const DialogType dialog_type,
      const base::Callback<void(const FormStructure*)>& callback)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     ssl_status,
                                     metric_logger,
                                     dialog_type,
                                     callback) {}
  virtual ~TestAutofillDialogController() {}

  virtual AutofillDialogView* CreateView() OVERRIDE {
    return new TestAutofillDialogView();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogControllerTest : public testing::Test {
 public:
  AutofillDialogControllerTest() {}
  virtual ~AutofillDialogControllerTest() {}

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    FormFieldData field;
    field.autocomplete_attribute = "cc-number";
    FormData form_data;
    form_data.fields.push_back(field);

    profile()->GetPrefs()->SetBoolean(
        prefs::kAutofillDialogPayWithoutWallet, false);
    profile()->CreateRequestContext();
    test_web_contents_.reset(
        content::WebContentsTester::CreateTestWebContents(profile(), NULL));

    base::Callback<void(const FormStructure*)> callback;
    AutofillMetrics metrics;
    controller_ = new TestAutofillDialogController(
        test_web_contents_.get(),
        form_data,
        GURL(),
        content::SSLStatus(),
        metrics,
        DIALOG_TYPE_REQUEST_AUTOCOMPLETE,
        callback);
    controller_->Show();
  }

  virtual void TearDown() OVERRIDE {
    controller_->ViewClosed();
  }

 protected:
  AutofillDialogController* controller() { return controller_; }

 private:
  TestingProfile* profile() { return environment_.profile(); }

  // This sets up a bunch of threads which are necessary for classes like
  // TestWebContents and URLRequestContextGetter not to fall over.
  extensions::TestExtensionEnvironment environment_;

  // The controller owns itself.
  TestAutofillDialogController* controller_;

  scoped_ptr<content::WebContents> test_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

}  // namespace

// This test makes sure nothing falls over when fields are being validity-
// checked.
TEST_F(AutofillDialogControllerTest, ValidityCheck) {
  const DialogSection sections[] = {
    SECTION_EMAIL,
    SECTION_CC,
    SECTION_BILLING,
    SECTION_CC_BILLING,
    SECTION_SHIPPING
  };

  for (size_t i = 0; i < arraysize(sections); ++i) {
    DialogSection section = sections[i];
    const DetailInputs& shipping_inputs =
        controller()->RequestedFieldsForSection(section);
    for (DetailInputs::const_iterator iter = shipping_inputs.begin();
         iter != shipping_inputs.end(); ++iter) {
      controller()->InputIsValid(iter->type, string16());
    }
  }
}

}  // namespace autofill
