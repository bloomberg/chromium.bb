// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_

#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "ui/gfx/geometry/rect.h"

class GURL;

namespace blink {
class WebDocument;
class WebElement;
class WebFormElement;
class WebFormControlElement;
class WebFrame;
class WebInputElement;
class WebNode;
}

namespace autofill {

struct FormData;
struct FormFieldData;
struct WebElementDescriptor;

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

// The maximum number of form fields we are willing to parse, due to
// computational costs.  Several examples of forms with lots of fields that are
// not relevant to Autofill: (1) the Netflix queue; (2) the Amazon wishlist;
// (3) router configuration pages; and (4) other configuration pages, e.g. for
// Google code project settings.
extern const size_t kMaxParseableFields;

// Returns true if |element| is a month input element.
bool IsMonthInput(const blink::WebInputElement* element);

// Returns true if |element| is a text input element.
bool IsTextInput(const blink::WebInputElement* element);

// Returns true if |element| is a select element.
bool IsSelectElement(const blink::WebFormControlElement& element);

// Returns true if |element| is a textarea element.
bool IsTextAreaElement(const blink::WebFormControlElement& element);

// Returns true if |element| is a checkbox or a radio button element.
bool IsCheckableElement(const blink::WebInputElement* element);

// Returns true if |element| is one of the input element types that can be
// autofilled. {Text, Radiobutton, Checkbox}.
bool IsAutofillableInputElement(const blink::WebInputElement* element);

// Recursively checks whether |node| or any of its children have a non-empty
// bounding box.
bool IsWebNodeVisible(const blink::WebNode& node);

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
const base::string16 GetFormIdentifier(const blink::WebFormElement& form);

// Returns all the auto-fillable form control elements in |control_elements|.
std::vector<blink::WebFormControlElement> ExtractAutofillableElementsFromSet(
    const blink::WebVector<blink::WebFormControlElement>& control_elements,
    RequirementsMask requirements);

// Returns all the auto-fillable form control elements in |form_element|.
std::vector<blink::WebFormControlElement> ExtractAutofillableElementsInForm(
    const blink::WebFormElement& form_element,
    RequirementsMask requirements);

// Fills out a FormField object from a given WebFormControlElement.
// |extract_mask|: See the enum ExtractMask above for details.
void WebFormControlElementToFormField(
    const blink::WebFormControlElement& element,
    ExtractMask extract_mask,
    FormFieldData* field);

// Fills |form| with the FormData object corresponding to the |form_element|.
// If |field| is non-NULL, also fills |field| with the FormField object
// corresponding to the |form_control_element|.
// |extract_mask| controls what data is extracted.
// Returns true if |form| is filled out; it's possible that the |form_element|
// won't meet the |requirements|.  Also returns false if there are no fields or
// too many fields in the |form|.
bool WebFormElementToFormData(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& form_control_element,
    RequirementsMask requirements,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field);

// Get all form control elements from |elements| that are not part of a form.
// If |fieldsets| is not NULL, also append the fieldsets encountered that are
// not part of a form.
std::vector<blink::WebFormControlElement>
GetUnownedAutofillableFormFieldElements(
    const blink::WebElementCollection& elements,
    std::vector<blink::WebElement>* fieldsets);

// Fills |form| with the form data derived from |fieldsets|, |control_elements|
// and |origin|. If |field| is non-NULL, fill it with the FormField
// representation for |element|.
// |extract_mask| usage and the return value are the same as
// WebFormElementToFormData() above.
bool UnownedFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const GURL& origin,
    RequirementsMask requirements,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field);

// Finds the form that contains |element| and returns it in |form|.  If |field|
// is non-NULL, fill it with the FormField representation for |element|.
// Returns false if the form is not found or cannot be serialized.
bool FindFormAndFieldForFormControlElement(
    const blink::WebFormControlElement& element,
    FormData* form,
    FormFieldData* field,
    RequirementsMask requirements);

// Fills the form represented by |form|.  |element| is the input element that
// initiated the auto-fill process.
void FillForm(const FormData& form,
              const blink::WebFormControlElement& element);

// Fills focusable and non-focusable form control elements within |form_element|
// with field data from |form_data|.
void FillFormIncludingNonFocusableElements(
    const FormData& form_data,
    const blink::WebFormElement& form_element);

// Previews the form represented by |form|.  |element| is the input element that
// initiated the preview process.
void PreviewForm(const FormData& form,
                 const blink::WebFormControlElement& element);

// Clears the placeholder values and the auto-filled background for any fields
// in the form containing |node| that have been previewed.  Resets the
// autofilled state of |node| to |was_autofilled|.  Returns false if the form is
// not found.
bool ClearPreviewedFormWithElement(const blink::WebFormControlElement& element,
                                   bool was_autofilled);

// Checks if the webpage is empty.
// This kind of webpage is considered as empty:
// <html>
//    <head>
//    <head/>
//    <body>
//    <body/>
// <html/>
// Meta, script and title tags don't influence the emptiness of a webpage.
bool IsWebpageEmpty(const blink::WebFrame* frame);

// This function checks whether the children of |element|
// are of the type <script>, <meta>, or <title>.
bool IsWebElementEmpty(const blink::WebElement& element);

// Return a gfx::RectF that is the bounding box for |element| scaled by |scale|.
gfx::RectF GetScaledBoundingBox(float scale, blink::WebElement* element);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_
