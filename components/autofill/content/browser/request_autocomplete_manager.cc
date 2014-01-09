// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/request_autocomplete_manager.h"

#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace autofill {

RequestAutocompleteManager::RequestAutocompleteManager(
    AutofillDriverImpl* autofill_driver)
    : autofill_driver_(autofill_driver),
      weak_ptr_factory_(this) {
  DCHECK(autofill_driver_);
}

RequestAutocompleteManager::~RequestAutocompleteManager() {}

void RequestAutocompleteManager::OnRequestAutocomplete(
    const FormData& form,
    const GURL& frame_url) {
  if (!IsValidFormData(form))
    return;

  if (!autofill_driver_->autofill_manager()->IsAutofillEnabled()) {
    ReturnAutocompleteResult(
        blink::WebFormElement::AutocompleteResultErrorDisabled,
        FormData());
    return;
  }

  base::Callback<void(const FormStructure*)> callback =
      base::Bind(&RequestAutocompleteManager::ReturnAutocompleteData,
                 weak_ptr_factory_.GetWeakPtr());
  ShowRequestAutocompleteDialog(form, frame_url, callback);
}

void RequestAutocompleteManager::ReturnAutocompleteResult(
    blink::WebFormElement::AutocompleteResult result,
    const FormData& form_data) {
  // autofill_driver_->GetWebContents() will be NULL when the interactive
  // autocomplete is closed due to a tab or browser window closing.
  if (!autofill_driver_->GetWebContents())
    return;

  content::RenderViewHost* host =
      autofill_driver_->GetWebContents()->GetRenderViewHost();
  if (!host)
    return;

  host->Send(new AutofillMsg_RequestAutocompleteResult(host->GetRoutingID(),
                                                       result,
                                                       form_data));
}

void RequestAutocompleteManager::ReturnAutocompleteData(
    const FormStructure* result) {
  if (!result) {
    ReturnAutocompleteResult(
        blink::WebFormElement::AutocompleteResultErrorCancel, FormData());
  } else {
    ReturnAutocompleteResult(blink::WebFormElement::AutocompleteResultSuccess,
                             result->ToFormData());
  }
}

void RequestAutocompleteManager::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const base::Callback<void(const FormStructure*)>& callback) {
  AutofillManagerDelegate* delegate =
      autofill_driver_->autofill_manager()->delegate();
  delegate->ShowRequestAutocompleteDialog(form, source_url, callback);
}

}  // namespace autofill
