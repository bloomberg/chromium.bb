// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocheckout_manager.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "googleurl/src/gurl.h"

using content::SSLStatus;
using content::WebContents;

namespace {

// Build FormFieldData based on the supplied |autocomplete_attribute|. Will
// fill rest of properties with default values.
FormFieldData BuildField(const std::string& autocomplete_attribute) {
  FormFieldData field;
  field.name = string16();
  field.value = string16();
  field.autocomplete_attribute = autocomplete_attribute;
  field.form_control_type = "text";
  return field;
}

// Build Autocheckout specific form data to be consumed by
// AutofillDialogController to show the Autocheckout specific UI.
FormData BuildAutocheckoutFormData() {
  FormData formdata;
  formdata.fields.push_back(BuildField("name"));
  formdata.fields.push_back(BuildField("email"));
  formdata.fields.push_back(BuildField("cc-name"));
  formdata.fields.push_back(BuildField("cc-number"));
  formdata.fields.push_back(BuildField("cc-exp"));
  formdata.fields.push_back(BuildField("cc-csc"));
  formdata.fields.push_back(BuildField("billing street-address"));
  formdata.fields.push_back(BuildField("billing locality"));
  formdata.fields.push_back(BuildField("billing region"));
  formdata.fields.push_back(BuildField("billing country"));
  formdata.fields.push_back(BuildField("billing postal-code"));
  formdata.fields.push_back(BuildField("shipping street-address"));
  formdata.fields.push_back(BuildField("shipping locality"));
  formdata.fields.push_back(BuildField("shipping region"));
  formdata.fields.push_back(BuildField("shipping country"));
  formdata.fields.push_back(BuildField("shipping postal-code"));
  return formdata;
}

}  // namespace

AutocheckoutManager::AutocheckoutManager(AutofillManager* autofill_manager)
  : autofill_manager_(autofill_manager),
    ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

AutocheckoutManager::~AutocheckoutManager() {
}

void AutocheckoutManager::ShowAutocheckoutDialog(
    const GURL& frame_url,
    const SSLStatus& ssl_status) {
  base::Callback<void(const FormStructure*)> callback =
      base::Bind(&AutocheckoutManager::ReturnAutocheckoutData,
                 weak_ptr_factory_.GetWeakPtr());
  autofill::AutofillDialogControllerImpl* controller =
      new autofill::AutofillDialogControllerImpl(
          autofill_manager_->GetWebContents(),
          BuildAutocheckoutFormData(),
          frame_url,
          ssl_status,
          callback);
  controller->Show();
}

void AutocheckoutManager::ReturnAutocheckoutData(const FormStructure* result) {
  // TODO(ramankk): Parse the response FormStructure.
}
