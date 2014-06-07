// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/request_autocomplete_manager.h"

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "url/gurl.h"

namespace autofill {

namespace {

blink::WebFormElement::AutocompleteResult ToWebkitAutocompleteResult(
    AutofillClient::RequestAutocompleteResult result) {
  switch(result) {
    case AutofillClient::AutocompleteResultSuccess:
      return blink::WebFormElement::AutocompleteResultSuccess;
    case AutofillClient::AutocompleteResultErrorDisabled:
      return blink::WebFormElement::AutocompleteResultErrorDisabled;
    case AutofillClient::AutocompleteResultErrorCancel:
      return blink::WebFormElement::AutocompleteResultErrorCancel;
    case AutofillClient::AutocompleteResultErrorInvalid:
      return blink::WebFormElement::AutocompleteResultErrorInvalid;
  }

  NOTREACHED();
  return blink::WebFormElement::AutocompleteResultErrorDisabled;
}

}  // namespace

RequestAutocompleteManager::RequestAutocompleteManager(
    ContentAutofillDriver* autofill_driver)
    : autofill_driver_(autofill_driver), weak_ptr_factory_(this) {
  DCHECK(autofill_driver_);
}

RequestAutocompleteManager::~RequestAutocompleteManager() {}

void RequestAutocompleteManager::OnRequestAutocomplete(
    const FormData& form,
    const GURL& frame_url) {
  if (!IsValidFormData(form))
    return;

  AutofillClient::ResultCallback callback =
      base::Bind(&RequestAutocompleteManager::ReturnAutocompleteResult,
                 weak_ptr_factory_.GetWeakPtr());
  ShowRequestAutocompleteDialog(form, frame_url, callback);
}

void RequestAutocompleteManager::OnCancelRequestAutocomplete() {
  autofill_driver_->autofill_manager()->client()
      ->HideRequestAutocompleteDialog();
}

void RequestAutocompleteManager::ReturnAutocompleteResult(
    AutofillClient::RequestAutocompleteResult result,
    const base::string16& debug_message,
    const FormStructure* form_structure) {
  // autofill_driver_->GetWebContents() will be NULL when the interactive
  // autocomplete is closed due to a tab or browser window closing.
  if (!autofill_driver_->GetWebContents())
    return;

  content::RenderViewHost* host =
      autofill_driver_->GetWebContents()->GetRenderViewHost();
  if (!host)
    return;

  FormData form_data;
  if (form_structure) {
    form_data = form_structure->ToFormData();
    for (size_t i = 0; i < form_data.fields.size(); ++i) {
      if(!form_data.fields[i].value.empty())
        form_data.fields[i].is_autofilled = true;
    }
  }

  host->Send(new AutofillMsg_RequestAutocompleteResult(
      host->GetRoutingID(),
      ToWebkitAutocompleteResult(result),
      debug_message,
      form_data));
}

void RequestAutocompleteManager::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const AutofillClient::ResultCallback& callback) {
  AutofillClient* client = autofill_driver_->autofill_manager()->client();
  client->ShowRequestAutocompleteDialog(form, source_url, callback);
}

}  // namespace autofill
