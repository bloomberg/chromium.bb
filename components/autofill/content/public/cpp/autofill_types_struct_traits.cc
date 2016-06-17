// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/public/cpp/autofill_types_struct_traits.h"

#include "base/i18n/rtl.h"
#include "url/mojo/url_gurl_struct_traits.h"

using namespace autofill;

namespace mojo {

mojom::CheckStatus
EnumTraits<mojom::CheckStatus, FormFieldData::CheckStatus>::ToMojom(
    FormFieldData::CheckStatus input) {
  switch (input) {
    case FormFieldData::CheckStatus::NOT_CHECKABLE:
      return mojom::CheckStatus::NOT_CHECKABLE;
    case FormFieldData::CheckStatus::CHECKABLE_BUT_UNCHECKED:
      return mojom::CheckStatus::CHECKABLE_BUT_UNCHECKED;
    case FormFieldData::CheckStatus::CHECKED:
      return mojom::CheckStatus::CHECKED;
  }

  NOTREACHED();
  return mojom::CheckStatus::NOT_CHECKABLE;
}

bool EnumTraits<mojom::CheckStatus, FormFieldData::CheckStatus>::FromMojom(
    mojom::CheckStatus input,
    FormFieldData::CheckStatus* output) {
  switch (input) {
    case mojom::CheckStatus::NOT_CHECKABLE:
      *output = FormFieldData::CheckStatus::NOT_CHECKABLE;
      return true;
    case mojom::CheckStatus::CHECKABLE_BUT_UNCHECKED:
      *output = FormFieldData::CheckStatus::CHECKABLE_BUT_UNCHECKED;
      return true;
    case mojom::CheckStatus::CHECKED:
      *output = FormFieldData::CheckStatus::CHECKED;
      return true;
  }

  NOTREACHED();
  return false;
}

mojom::RoleAttribute
EnumTraits<mojom::RoleAttribute, FormFieldData::RoleAttribute>::ToMojom(
    FormFieldData::RoleAttribute input) {
  switch (input) {
    case FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION:
      return mojom::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION;
    case FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_OTHER:
      return mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
  }

  NOTREACHED();
  return mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
}

bool EnumTraits<mojom::RoleAttribute, FormFieldData::RoleAttribute>::FromMojom(
    mojom::RoleAttribute input,
    FormFieldData::RoleAttribute* output) {
  switch (input) {
    case mojom::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION:
      *output = FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION;
      return true;
    case mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER:
      *output = FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
      return true;
  }

  NOTREACHED();
  return false;
}

mojom::TextDirection
EnumTraits<mojom::TextDirection, base::i18n::TextDirection>::ToMojom(
    base::i18n::TextDirection input) {
  switch (input) {
    case base::i18n::TextDirection::UNKNOWN_DIRECTION:
      return mojom::TextDirection::UNKNOWN_DIRECTION;
    case base::i18n::TextDirection::RIGHT_TO_LEFT:
      return mojom::TextDirection::RIGHT_TO_LEFT;
    case base::i18n::TextDirection::LEFT_TO_RIGHT:
      return mojom::TextDirection::LEFT_TO_RIGHT;
    case base::i18n::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS:
      return mojom::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS;
  }

  NOTREACHED();
  return mojom::TextDirection::UNKNOWN_DIRECTION;
}

bool EnumTraits<mojom::TextDirection, base::i18n::TextDirection>::FromMojom(
    mojom::TextDirection input,
    base::i18n::TextDirection* output) {
  switch (input) {
    case mojom::TextDirection::UNKNOWN_DIRECTION:
      *output = base::i18n::TextDirection::UNKNOWN_DIRECTION;
      return true;
    case mojom::TextDirection::RIGHT_TO_LEFT:
      *output = base::i18n::TextDirection::RIGHT_TO_LEFT;
      return true;
    case mojom::TextDirection::LEFT_TO_RIGHT:
      *output = base::i18n::TextDirection::LEFT_TO_RIGHT;
      return true;
    case mojom::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS:
      *output = base::i18n::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<mojom::FormFieldData, FormFieldData>::Read(
    mojom::FormFieldDataDataView data,
    FormFieldData* out) {
  if (!data.ReadLabel(&out->label))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadValue(&out->value))
    return false;

  if (!data.ReadFormControlType(&out->form_control_type))
    return false;
  if (!data.ReadAutocompleteAttribute(&out->autocomplete_attribute))
    return false;

  if (!data.ReadPlaceholder(&out->placeholder))
    return false;

  out->max_length = data.max_length();
  out->is_autofilled = data.is_autofilled();

  if (!data.ReadCheckStatus(&out->check_status))
    return false;

  out->is_focusable = data.is_focusable();
  out->should_autocomplete = data.should_autocomplete();

  if (!data.ReadRole(&out->role))
    return false;

  if (!data.ReadTextDirection(&out->text_direction))
    return false;

  if (!data.ReadOptionValues(&out->option_values))
    return false;
  if (!data.ReadOptionContents(&out->option_contents))
    return false;

  return true;
}

// static
bool StructTraits<mojom::FormData, FormData>::Read(mojom::FormDataDataView data,
                                                   FormData* out) {
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadOrigin(&out->origin))
    return false;
  if (!data.ReadAction(&out->action))
    return false;

  out->is_form_tag = data.is_form_tag();
  out->is_formless_checkout = data.is_formless_checkout();

  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

// static
bool StructTraits<mojom::FormFieldDataPredictions, FormFieldDataPredictions>::
    Read(mojom::FormFieldDataPredictionsDataView data,
         FormFieldDataPredictions* out) {
  if (!data.ReadField(&out->field))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadHeuristicType(&out->heuristic_type))
    return false;
  if (!data.ReadServerType(&out->server_type))
    return false;
  if (!data.ReadOverallType(&out->overall_type))
    return false;
  if (!data.ReadParseableName(&out->parseable_name))
    return false;

  return true;
}

// static
bool StructTraits<mojom::FormDataPredictions, FormDataPredictions>::Read(
    mojom::FormDataPredictionsDataView data,
    FormDataPredictions* out) {
  if (!data.ReadData(&out->data))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

}  // namespace mojo
