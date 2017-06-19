// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler_proxy.h"

#include "components/autofill/core/browser/autofill_provider.h"

namespace autofill {

using base::TimeTicks;

AutofillHandlerProxy::AutofillHandlerProxy(AutofillDriver* driver,
                                           AutofillProvider* provider)
    : AutofillHandler(driver), provider_(provider), weak_ptr_factory_(this) {}

AutofillHandlerProxy::~AutofillHandlerProxy() {}

bool AutofillHandlerProxy::OnWillSubmitFormImpl(const FormData& form,
                                                const TimeTicks timestamp) {
  return provider_->OnWillSubmitForm(this, form, timestamp);
}

void AutofillHandlerProxy::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const TimeTicks timestamp) {
  provider_->OnTextFieldDidChange(this, form, field, bounding_box, timestamp);
}

void AutofillHandlerProxy::OnQueryFormFieldAutofillImpl(
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnQueryFormFieldAutofill(this, query_id, form, field,
                                      bounding_box);
}

void AutofillHandlerProxy::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnFocusOnFormField(this, form, field, bounding_box);
}

void AutofillHandlerProxy::OnFocusNoLongerOnForm() {
  provider_->OnFocusNoLongerOnForm(this);
}

void AutofillHandlerProxy::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  provider_->OnDidFillAutofillFormData(this, form, timestamp);
}

void AutofillHandlerProxy::OnDidPreviewAutofillFormData() {}

void AutofillHandlerProxy::OnFormsSeen(const std::vector<FormData>& forms,
                                       const base::TimeTicks timestamp) {}

bool AutofillHandlerProxy::OnFormSubmitted(const FormData& form) {
  return false;
}

void AutofillHandlerProxy::OnDidEndTextFieldEditing() {}

void AutofillHandlerProxy::OnHidePopup() {}

void AutofillHandlerProxy::OnSetDataList(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {}

void AutofillHandlerProxy::Reset() {
  provider_->Reset(this);
}

}  // namespace autofill
