// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_

#include <limits>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/common/button_title_type.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/submission_indicator_event.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

// Pair of a button title (e.g. "Register") and its type (e.g.
// INPUT_ELEMENT_SUBMIT_TYPE).
using ButtonTitleInfo = std::pair<base::string16, ButtonTitleType>;

// List of button titles of a given form.
using ButtonTitleList = std::vector<ButtonTitleInfo>;

// Holds information about a form to be filled and/or submitted.
struct FormData {
  static constexpr uint32_t kNotSetFormRendererId =
      std::numeric_limits<uint32_t>::max();

  FormData();
  FormData(const FormData& data);
  ~FormData();

  // Returns true if two forms are the same, not counting the values of the
  // form elements.
  bool SameFormAs(const FormData& other) const;

  // Same as SameFormAs() except calling FormFieldData.SimilarFieldAs() to
  // compare fields.
  bool SimilarFormAs(const FormData& other) const;

  // If |form| is the same as this from the POV of dynamic refills.
  bool DynamicallySameFormAs(const FormData& form) const;

  // Note: operator==() performs a full-field-comparison(byte by byte), this is
  // different from SameFormAs(), which ignores comparison for those "values" of
  // all form fields, just like what FormFieldData::SameFieldAs() ignores.
  bool operator==(const FormData& form) const;
  bool operator!=(const FormData& form) const;
  // Allow FormData to be a key in STL containers.
  bool operator<(const FormData& form) const;

  // The id attribute of the form.
  base::string16 id_attribute;

  // The name attribute of the form.
  base::string16 name_attribute;

  // The name by which autofill knows this form. This is generally either the
  // name attribute or the id_attribute value, which-ever is non-empty with
  // priority given to the name_attribute. This value is used when computing
  // form signatures.
  // TODO(crbug/896689): remove this and use attributes/unique_id instead.
  base::string16 name;
  // Titles of form's buttons.
  ButtonTitleList button_titles;
  // The URL (minus query parameters) containing the form.
  GURL origin;
  // The action target of the form.
  GURL action;
  // The URL of main frame containing this form.
  url::Origin main_frame_origin;
  // True if this form is a form tag.
  bool is_form_tag;
  // True if the form is made of unowned fields (i.e., not within a <form> tag)
  // in what appears to be a checkout flow. This attribute is only calculated
  // and used if features::kAutofillRestrictUnownedFieldsToFormlessCheckout is
  // enabled, to prevent heuristics from running on formless non-checkout.
  bool is_formless_checkout;
  //  Unique renderer id which is returned by function
  //  WebFormElement::UniqueRendererFormId(). It is not persistant between page
  //  loads, so it is not saved and not used in comparison in SameFormAs().
  uint32_t unique_renderer_id = kNotSetFormRendererId;
  // The type of the event that was taken as an indication that this form is
  // being or has already been submitted. This field is filled only in Password
  // Manager for submitted password forms.
  SubmissionIndicatorEvent submission_event = SubmissionIndicatorEvent::NONE;
  // A vector of all the input fields in the form.
  std::vector<FormFieldData> fields;
  // Contains unique renderer IDs of text elements which are predicted to be
  // usernames. The order matters: elements are sorted in descending likelihood
  // of being a username (the first one is the most likely username). Can
  // contain IDs of elements which are not in |fields|. This is only used during
  // parsing into PasswordForm, and hence not serialised for storage.
  std::vector<uint32_t> username_predictions;
};

// For testing.
std::ostream& operator<<(std::ostream& os, const FormData& form);

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms. Serialized data is appended to |pickle|.
void SerializeFormData(const FormData& form_data, base::Pickle* pickle);
// Deserialize FormData. This assumes that |iter| is currently pointing to
// the part of a pickle created by SerializeFormData. Returns true on success.
bool DeserializeFormData(base::PickleIterator* iter, FormData* form_data);

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms in base64 string. It is useful since in some
// cases we need to store C strings without embedded '\0' symbols.
void SerializeFormDataToBase64String(const FormData& form_data,
                                     std::string* output);
// Deserialize FormData. Returns true on success.
bool DeserializeFormDataFromBase64String(const base::StringPiece& input,
                                         FormData* form_data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
