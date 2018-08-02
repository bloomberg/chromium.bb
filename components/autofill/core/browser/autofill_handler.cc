// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler.h"

#include "base/containers/adapters.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/signatures_util.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kAutofillHandlerMaxFormCacheSize = 100;

}  // namespace

using base::TimeTicks;

AutofillHandler::AutofillHandler(AutofillDriver* driver) : driver_(driver) {}

AutofillHandler::~AutofillHandler() {}

void AutofillHandler::OnFormSubmitted(const FormData& form,
                                      bool known_success,
                                      SubmissionSource source,
                                      base::TimeTicks timestamp) {
  if (IsValidFormData(form))
    OnFormSubmittedImpl(form, known_success, source, timestamp);
}

void AutofillHandler::OnFormsSeen(const std::vector<FormData>& forms,
                                  const base::TimeTicks timestamp) {
  if (!IsValidFormDataVector(forms) || !driver_->RendererIsAvailable())
    return;

  // This should be called even forms is empty, AutofillProviderAndroid uses
  // this event to detect form submission.
  if (!ShouldParseForms(forms, timestamp))
    return;

  if (forms.empty())
    return;

  std::vector<FormStructure*> form_structures;
  for (const FormData& form : forms) {
    const auto parse_form_start_time = TimeTicks::Now();
    FormStructure* form_structure = nullptr;
    if (!ParseForm(form, /*cached_form=*/nullptr, &form_structure))
      continue;
    DCHECK(form_structure);
    if (form_structure == nullptr)
      continue;
    form_structures.push_back(form_structure);
    AutofillMetrics::LogParseFormTiming(TimeTicks::Now() -
                                        parse_form_start_time);
  }
  if (!form_structures.empty())
    OnFormsParsed(form_structures, timestamp);
}

void AutofillHandler::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           const TimeTicks timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnTextFieldDidChangeImpl(form, field, transformed_box, timestamp);
}

void AutofillHandler::OnTextFieldDidScroll(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnTextFieldDidScrollImpl(form, field, transformed_box);
}

void AutofillHandler::OnSelectControlDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnSelectControlDidChangeImpl(form, field, transformed_box);
}

void AutofillHandler::OnQueryFormFieldAutofill(
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnQueryFormFieldAutofillImpl(query_id, form, field, transformed_box,
                               autoselect_first_suggestion);
}

void AutofillHandler::OnFocusOnFormField(const FormData& form,
                                         const FormFieldData& field,
                                         const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnFocusOnFormFieldImpl(form, field, transformed_box);
}

void AutofillHandler::SendFormDataToRenderer(
    int query_id,
    AutofillDriver::RendererFormDataAction action,
    const FormData& data) {
  driver_->SendFormDataToRenderer(query_id, action, data);
}

bool AutofillHandler::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Find the FormStructure that corresponds to |form|.
  // If we do not have this form in our cache but it is parseable, we'll add it
  // in the call to |UpdateCachedForm()|.
  if (!FindCachedForm(form, form_structure) &&
      !FormStructure(form).ShouldBeParsed()) {
    return false;
  }

  // Update the cached form to reflect any dynamic changes to the form data, if
  // necessary.
  if (!UpdateCachedForm(form, *form_structure, form_structure))
    return false;

  // No data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutofillField that corresponds to |field|.
  *autofill_field = nullptr;
  for (const auto& current : **form_structure) {
    if (current->SameFieldAs(field)) {
      *autofill_field = current.get();
      break;
    }
  }

  // Even though we always update the cache, the field might not exist if the
  // website disables autocomplete while the user is interacting with the form.
  // See http://crbug.com/160476
  return *autofill_field != nullptr;
}

bool AutofillHandler::UpdateCachedForm(const FormData& live_form,
                                       const FormStructure* cached_form,
                                       FormStructure** updated_form) {
  bool needs_update =
      (!cached_form || live_form.fields.size() != cached_form->field_count());
  for (size_t i = 0; !needs_update && i < cached_form->field_count(); ++i)
    needs_update = !cached_form->field(i)->SameFieldAs(live_form.fields[i]);

  if (!needs_update)
    return true;

  // Note: We _must not_ remove the original version of the cached form from
  // the list of |form_structures_|. Otherwise, we break parsing of the
  // crowdsourcing server's response to our query.
  if (!ParseForm(live_form, cached_form, updated_form))
    return false;

  // Annotate the updated form with its predicted types.
  driver_->SendAutofillTypePredictionsToRenderer({*updated_form});

  return true;
}

bool AutofillHandler::FindCachedForm(const FormData& form,
                                     FormStructure** form_structure) const {
  // Find the FormStructure that corresponds to |form|.
  // Scan backward through the cached |form_structures_|, as updated versions of
  // forms are added to the back of the list, whereas original versions of these
  // forms might appear toward the beginning of the list.  The communication
  // protocol with the crowdsourcing server does not permit us to discard the
  // original versions of the forms.
  *form_structure = nullptr;
  const auto& form_signature = autofill::CalculateFormSignature(form);
  for (auto& cur_form : base::Reversed(form_structures_)) {
    if (cur_form->form_signature() == form_signature || *cur_form == form) {
      *form_structure = cur_form.get();

      // The same form might be cached with multiple field counts: in some
      // cases, non-autofillable fields are filtered out, whereas in other cases
      // they are not.  To avoid thrashing the cache, keep scanning until we
      // find a cached version with the same number of fields, if there is one.
      if (cur_form->field_count() == form.fields.size())
        break;
    }
  }

  if (!(*form_structure))
    return false;

  return true;
}

bool AutofillHandler::ParseForm(const FormData& form,
                                const FormStructure* cached_form,
                                FormStructure** parsed_form_structure) {
  DCHECK(parsed_form_structure);
  if (form_structures_.size() >= kAutofillHandlerMaxFormCacheSize)
    return false;

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  if (!form_structure->ShouldBeParsed()) {
    return false;
  }

  if (cached_form) {
    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(*cached_form,
                                      /*apply_is_autofilled=*/true,
                                      /*only_server_and_autofill_state=*/true);
  }

  form_structure->DetermineHeuristicTypes();

  // Ownership is transferred to |form_structures_| which maintains it until
  // the manager is Reset() or destroyed. It is safe to use references below
  // as long as receivers don't take ownership.
  form_structure->set_form_parsed_timestamp(TimeTicks::Now());
  form_structures_.push_back(std::move(form_structure));
  *parsed_form_structure = form_structures_.back().get();
  return true;
}

void AutofillHandler::Reset() {
  form_structures_.clear();
}

}  // namespace autofill
