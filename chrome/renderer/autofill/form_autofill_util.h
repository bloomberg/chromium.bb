// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FORM_AUTOFILL_UTIL_H_
#define CHROME_RENDERER_AUTOFILL_FORM_AUTOFILL_UTIL_H_
#pragma once

#include <vector>

#include "base/string16.h"

namespace webkit_glue {
struct FormData;
struct FormDataPredictions;
struct FormField;
}  // namespace webkit_glue

namespace WebKit {
class WebFormElement;
class WebFormControlElement;
class WebInputElement;
class WebSelectElement;
}  // namespace WebKit

namespace autofill {

// A bit field mask for form requirements.
enum RequirementsMask {
  REQUIRE_NONE         = 0,  // No requirements.
  REQUIRE_AUTOCOMPLETE = 1,  // Require that autocomplete != off.
};

// A bit field mask to extract data from WebFormControlElement.
enum ExtractMask {
  EXTRACT_NONE        = 0,
  EXTRACT_VALUE       = 1 << 0,  // Extract value from WebFormControlElement.
  EXTRACT_OPTION_TEXT = 1 << 1,  // Extract option text from
                                 // WebFormSelectElement. Only valid when
                                 // |EXTRACT_VALUE| is set.
                                 // This is used for form submission where
                                 // human readable value is captured.
  EXTRACT_OPTIONS     = 1 << 2,  // Extract options from
                                 // WebFormControlElement.
};

// Returns true if |element| is a text input element.
bool IsTextInput(const WebKit::WebInputElement* element);

// Returns true if |element| is a select element.
bool IsSelectElement(const WebKit::WebFormControlElement& element);

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
const string16 GetFormIdentifier(const WebKit::WebFormElement& form);

// Fills |autofillable_elements| with all the auto-fillable form control
// elements in |form_element|.
void ExtractAutofillableElements(
    const WebKit::WebFormElement& form_element,
    RequirementsMask requirements,
    std::vector<WebKit::WebFormControlElement>* autofillable_elements);

// Fills out a FormField object from a given WebFormControlElement.
// |extract_mask|: See the enum ExtractMask above for details.
void WebFormControlElementToFormField(
    const WebKit::WebFormControlElement& element,
    ExtractMask extract_mask,
    webkit_glue::FormField* field);

// Fills |form| with the FormData object corresponding to the |form_element|.
// If |field| is non-NULL, also fills |field| with the FormField object
// corresponding to the |form_control_element|.
// |extract_mask| controls what data is extracted.
// Returns true if |form| is filled out; it's possible that the |form_element|
// won't meet the |requirements|.  Also returns false if there are no fields
// in the |form|.
bool WebFormElementToFormData(
    const WebKit::WebFormElement& form_element,
    const WebKit::WebFormControlElement& form_control_element,
    RequirementsMask requirements,
    ExtractMask extract_mask,
    webkit_glue::FormData* form,
    webkit_glue::FormField* field);

// Finds the form that contains |element| and returns it in |form|.  Fills
// |field| with the |FormField| representation for element.
// Returns false if the form is not found.
bool FindFormAndFieldForInputElement(const WebKit::WebInputElement& element,
                                     webkit_glue::FormData* form,
                                     webkit_glue::FormField* field,
                                     RequirementsMask requirements);

// Fills the form represented by |form|.  |element| is the input element that
// initiated the auto-fill process.
void FillForm(const webkit_glue::FormData& form,
              const WebKit::WebInputElement& element);

// Previews the form represented by |form|.  |element| is the input element that
// initiated the preview process.
void PreviewForm(const webkit_glue::FormData& form,
                 const WebKit::WebInputElement& element);

// Clears the placeholder values and the auto-filled background for any fields
// in the form containing |node| that have been previewed.  Resets the
// autofilled state of |node| to |was_autofilled|.  Returns false if the form is
// not found.
bool ClearPreviewedFormWithElement(const WebKit::WebInputElement& element,
                                   bool was_autofilled);

// Returns true if |form| has any auto-filled fields.
bool FormWithElementIsAutofilled(const WebKit::WebInputElement& element);

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_FORM_AUTOFILL_UTIL_H_
