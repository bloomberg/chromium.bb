// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FORM_MANAGER_H_
#define CHROME_RENDERER_AUTOFILL_FORM_MANAGER_H_
#pragma once

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"

namespace webkit_glue {
struct FormData;
class FormField;
}  // namespace webkit_glue

namespace WebKit {
class WebFormControlElement;
class WebFrame;
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

  // Returns the corresponding label for |element|.  WARNING: This method can
  // potentially be very slow.  Do not use during any code paths where the page
  // is loading.
  static string16 LabelForElement(const WebKit::WebFormControlElement& element);

  // Fills out a FormData object from a given WebFormElement. If |get_values|
  // is true, the fields in |form| will have the values filled out. If
  // |get_options| is true, the fields in |form will have select options filled
  // out. Returns true if |form| is filled out; it's possible that |element|
  // won't meet the requirements in |requirements|. This also returns false if
  // there are no fields in |form|.
  // TODO(jhawkins): Remove the user of this in RenderView and move this to
  // private.
  static bool WebFormElementToFormData(const WebKit::WebFormElement& element,
                                       RequirementsMask requirements,
                                       ExtractMask extract_mask,
                                       webkit_glue::FormData* form);

  // Scans the DOM in |frame| extracting and storing forms.
  void ExtractForms(const WebKit::WebFrame* frame);

  // Returns a vector of forms in |frame| that match |requirements|.
  void GetFormsInFrame(const WebKit::WebFrame* frame,
                       RequirementsMask requirements,
                       std::vector<webkit_glue::FormData>* forms);

  // Finds the form that contains |element| and returns it in |form|. Returns
  // false if the form is not found.
  bool FindFormWithFormControlElement(
      const WebKit::WebFormControlElement& element,
      RequirementsMask requirements,
      webkit_glue::FormData* form);

  // Fills the form represented by |form|.  |form| should have the name set to
  // the name of the form to fill out, and the number of elements and values
  // must match the number of stored elements in the form. |node| is the form
  // control element that initiated the auto-fill process.
  // TODO(jhawkins): Is matching on name alone good enough?  It's possible to
  // store multiple forms with the same names from different frames.
  bool FillForm(const webkit_glue::FormData& form, const WebKit::WebNode& node);

  // Previews the form represented by |form|. |node| is the form control element
  // that initiated the preview process. Same conditions as FillForm.
  bool PreviewForm(const webkit_glue::FormData& form,
                   const WebKit::WebNode &node);

  // Clears the values of all input elements in the form that contains |node|.
  // Returns false if the form is not found.
  bool ClearFormWithNode(const WebKit::WebNode& node);

  // Clears the placeholder values and the auto-filled background for any fields
  // in the form containing |node| that have been previewed. Resets the
  // autofilled state of |node| to |was_autofilled|. Returns false if the form
  // is not found.
  bool ClearPreviewedFormWithNode(const WebKit::WebNode& node,
                                  bool was_autofilled);

  // Resets the stored set of forms.
  void Reset();

  // Resets the forms for the specified |frame|.
  void ResetFrame(const WebKit::WebFrame* frame);

  // Returns true if |form| has any auto-filled fields.
  bool FormWithNodeIsAutoFilled(const WebKit::WebNode& node);

 private:
  // Stores the WebFormElement and the form control elements for a form.
  // Original form values are stored so when we clear a form we can reset
  // "select-one" values to their original state.
  struct FormElement;

  // Type for cache of FormElement objects.
  typedef std::vector<FormElement*> FormElementList;

  // The callback type used by ForEachMatchingFormField().
  typedef Callback3<WebKit::WebFormControlElement*,
                    const webkit_glue::FormField*,
                    bool>::Type Callback;

  // Infers corresponding label for |element| from surrounding context in the
  // DOM.  Contents of preceding <p> tag or preceding text element found in
  // the form.
  static string16 InferLabelForElement(
      const WebKit::WebFormControlElement& element);

  // Finds the cached FormElement that contains |node|.
  bool FindCachedFormElementWithNode(const WebKit::WebNode& node,
                                     FormElement** form_element);

  // Uses the data in |form| to find the cached FormElement.
  bool FindCachedFormElement(const webkit_glue::FormData& form,
                             FormElement** form_element);

  // For each field in |data| that matches the corresponding field in |form|
  // and meets the |requirements|, |callback| is called with the actual
  // WebFormControlElement and the FormField data from |form|. The field that
  // matches |node| is not required to be empty if |requirements| includes
  // REQUIRE_EMPTY.  This method owns |callback|.
  void ForEachMatchingFormField(FormElement* form,
                                const WebKit::WebNode& node,
                                RequirementsMask requirements,
                                const webkit_glue::FormData& data,
                                Callback* callback);

  // A ForEachMatchingFormField() callback that sets |field|'s value using the
  // value in |data|.  This method also sets the autofill attribute, causing the
  // background to be yellow.
  void FillFormField(WebKit::WebFormControlElement* field,
                     const webkit_glue::FormField* data,
                     bool is_initiating_node);

  // A ForEachMatchingFormField() callback that sets |field|'s placeholder value
  // using the value in |data|, causing the test to be greyed-out.  This method
  // also sets the autofill attribute, causing the background to be yellow.
  void PreviewFormField(WebKit::WebFormControlElement* field,
                        const webkit_glue::FormField* data,
                        bool is_initiating_node);

  // The cached FormElement objects.
  FormElementList form_elements_;

  DISALLOW_COPY_AND_ASSIGN(FormManager);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_FORM_MANAGER_H_
