// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FORM_MANAGER_H_
#define CHROME_RENDERER_AUTOFILL_FORM_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "base/callback_old.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"

namespace webkit_glue {
struct FormData;
struct FormDataPredictions;
struct FormField;
}  // namespace webkit_glue

namespace WebKit {
class WebFormControlElement;
class WebFrame;
class WebInputElement;
class WebSelectElement;
}  // namespace WebKit

namespace autofill {

// Manages the forms in a RenderView.
class FormManager {
 public:
  // A bit field mask for form requirements.
  enum RequirementsMask {
    REQUIRE_NONE         = 0,       // No requirements.
    REQUIRE_AUTOCOMPLETE = 1 << 0,  // Require that autocomplete != off.
    REQUIRE_ENABLED      = 1 << 1,  // Require that disabled attribute is off.
    REQUIRE_EMPTY        = 1 << 2,  // Require that the fields are empty.
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

  FormManager();
  virtual ~FormManager();

  // Fills out a FormField object from a given WebFormControlElement.
  // |extract_mask|: See the enum ExtractMask above for details.
  static void WebFormControlElementToFormField(
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
  static bool WebFormElementToFormData(
      const WebKit::WebFormElement& form_element,
      const WebKit::WebFormControlElement& form_control_element,
      RequirementsMask requirements,
      ExtractMask extract_mask,
      webkit_glue::FormData* form,
      webkit_glue::FormField* field);

  // Finds the form that contains |element| and returns it in |form|.  Fills
  // |field| with the |FormField| representation for element.
  // Returns false if the form is not found.
  static bool FindFormAndFieldForFormControlElement(
      const WebKit::WebFormControlElement& element,
      webkit_glue::FormData* form,
      webkit_glue::FormField* field);

  // Fills the form represented by |form|.  |element| is the input element that
  // initiated the auto-fill process.
  static void FillForm(const webkit_glue::FormData& form,
                       const WebKit::WebInputElement& element);

  // Previews the form represented by |form|.  |element| is the input element
  // that initiated the preview process.
  static void PreviewForm(const webkit_glue::FormData& form,
                          const WebKit::WebInputElement& element);

  // Clears the placeholder values and the auto-filled background for any fields
  // in the form containing |node| that have been previewed. Resets the
  // autofilled state of |node| to |was_autofilled|. Returns false if the form
  // is not found.
  static bool ClearPreviewedFormWithElement(
      const WebKit::WebInputElement& element,
      bool was_autofilled);

  // Returns true if |form| has any auto-filled fields.
  static bool FormWithElementIsAutofilled(
      const WebKit::WebInputElement& element);

  // Scans the DOM in |frame| extracting and storing forms.
  // Returns a vector of the extracted forms.
  void ExtractForms(const WebKit::WebFrame& frame,
                    std::vector<webkit_glue::FormData>* forms);

  // Resets the forms for the specified |frame|.
  void ResetFrame(const WebKit::WebFrame& frame);

  // Clears the values of all input elements in the form that contains
  // |element|.  Returns false if the form is not found.
  bool ClearFormWithElement(const WebKit::WebInputElement& element);

  // For each field in the |form|, sets the field's placeholder text to the
  // field's overall predicted type.  Also sets the title to include the field's
  // heuristic type, server type, and signature; as well as the form's signature
  // and the experiment id for the server predictions.
  bool ShowPredictions(const webkit_glue::FormDataPredictions& form);

 private:
  // The cached web frames.
  std::set<const WebKit::WebFrame*> web_frames_;

  // The cached initial values for <select> elements.
  std::map<const WebKit::WebSelectElement, string16> initial_select_values_;

  DISALLOW_COPY_AND_ASSIGN(FormManager);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_FORM_MANAGER_H_
