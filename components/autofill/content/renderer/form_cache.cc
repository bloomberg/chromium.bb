// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSelectElement.h"
#include "ui/base/l10n/l10n_util.h"

using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebLocalFrame;
using blink::WebInputElement;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

namespace {

void LogDeprecationMessages(const WebFormControlElement& element) {
  std::string autocomplete_attribute =
      element.GetAttribute("autocomplete").Utf8();

  static const char* const deprecated[] = { "region", "locality" };
  for (const char* str : deprecated) {
    if (autocomplete_attribute.find(str) == std::string::npos)
      continue;
    std::string msg = std::string("autocomplete='") + str +
        "' is deprecated and will soon be ignored. See http://goo.gl/YjeSsW";
    WebConsoleMessage console_message = WebConsoleMessage(
        WebConsoleMessage::kLevelWarning, WebString::FromASCII(msg));
    element.GetDocument().GetFrame()->AddMessageToConsole(console_message);
  }
}

// Determines whether the form is interesting enough to send to the browser
// for further operations.
bool IsFormInteresting(const FormData& form, size_t num_editable_elements) {
  if (form.fields.empty())
    return false;

  // If the form has at least one field with an autocomplete attribute, it is a
  // candidate for autofill.
  bool all_fields_are_passwords = true;
  for (const FormFieldData& field : form.fields) {
    if (!field.autocomplete_attribute.empty())
      return true;
    if (field.form_control_type != "password")
      all_fields_are_passwords = false;
  }

  // If there are no autocomplete attributes, the form needs to have at least
  // the required number of editable fields for the prediction routines to be a
  // candidate for autofill.
  return num_editable_elements >= kRequiredFieldsForPredictionRoutines ||
         (all_fields_are_passwords &&
          num_editable_elements >=
              kRequiredFieldsForFormsWithOnlyPasswordFields);
}

}  // namespace

FormCache::FormCache(const WebLocalFrame& frame) : frame_(frame) {}

FormCache::~FormCache() {
}

std::vector<FormData> FormCache::ExtractNewForms() {
  std::vector<FormData> forms;
  WebDocument document = frame_.GetDocument();
  if (document.IsNull())
    return forms;

  initial_checked_state_.clear();
  initial_select_values_.clear();
  WebVector<WebFormElement> web_forms;
  document.Forms(web_forms);

  // Log an error message for deprecated attributes, but only the first time
  // the form is parsed.
  bool log_deprecation_messages = parsed_forms_.empty();

  const form_util::ExtractMask extract_mask =
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTIONS);

  size_t num_fields_seen = 0;
  for (size_t i = 0; i < web_forms.size(); ++i) {
    const WebFormElement& form_element = web_forms[i];

    std::vector<WebFormControlElement> control_elements =
        form_util::ExtractAutofillableElementsInForm(form_element);
    size_t num_editable_elements =
        ScanFormControlElements(control_elements, log_deprecation_messages);

    if (num_editable_elements == 0)
      continue;

    FormData form;
    if (!WebFormElementToFormData(form_element, WebFormControlElement(),
                                  nullptr, extract_mask, &form, nullptr)) {
      continue;
    }

    num_fields_seen += form.fields.size();
    if (num_fields_seen > form_util::kMaxParseableFields)
      return forms;

    if (!base::ContainsKey(parsed_forms_, form) &&
        IsFormInteresting(form, num_editable_elements)) {
      for (auto it = parsed_forms_.begin(); it != parsed_forms_.end(); ++it) {
        if (it->SameFormAs(form)) {
          parsed_forms_.erase(it);
          break;
        }
      }

      SaveInitialValues(control_elements);
      forms.push_back(form);
      parsed_forms_.insert(form);
    }
  }

  // Look for more parseable fields outside of forms.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document.All(),
                                                         &fieldsets);

  size_t num_editable_elements =
      ScanFormControlElements(control_elements, log_deprecation_messages);

  if (num_editable_elements == 0)
    return forms;

  FormData synthetic_form;
  if (!UnownedCheckoutFormElementsAndFieldSetsToFormData(
          fieldsets, control_elements, nullptr, document, extract_mask,
          &synthetic_form, nullptr)) {
    return forms;
  }

  num_fields_seen += synthetic_form.fields.size();
  if (num_fields_seen > form_util::kMaxParseableFields)
    return forms;

  if (!parsed_forms_.count(synthetic_form) &&
      IsFormInteresting(synthetic_form, num_editable_elements)) {
    SaveInitialValues(control_elements);
    forms.push_back(synthetic_form);
    parsed_forms_.insert(synthetic_form);
    parsed_forms_.erase(synthetic_form_);
    synthetic_form_ = synthetic_form;
  }
  return forms;
}

void FormCache::Reset() {
  synthetic_form_ = FormData();
  parsed_forms_.clear();
  initial_select_values_.clear();
  initial_checked_state_.clear();
}

bool FormCache::ClearFormWithElement(const WebFormControlElement& element) {
  WebFormElement form_element = element.Form();
  std::vector<WebFormControlElement> control_elements;
  if (form_element.IsNull()) {
    control_elements = form_util::GetUnownedAutofillableFormFieldElements(
        element.GetDocument().All(), nullptr);
  } else {
    control_elements =
        form_util::ExtractAutofillableElementsInForm(form_element);
  }
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement control_element = control_elements[i];
    // Don't modify the value of disabled fields.
    if (!control_element.IsEnabled())
      continue;

    // Don't clear field that was not autofilled
    if (!control_element.IsAutofilled())
      continue;

    control_element.SetAutofilled(false);

    WebInputElement* input_element = ToWebInputElement(&control_element);
    if (form_util::IsTextInput(input_element) ||
        form_util::IsMonthInput(input_element)) {
      input_element->SetAutofillValue(blink::WebString());

      // Clearing the value in the focused node (above) can cause selection
      // to be lost. We force selection range to restore the text cursor.
      if (element == *input_element) {
        int length = input_element->Value().length();
        input_element->SetSelectionRange(length, length);
      }
    } else if (form_util::IsTextAreaElement(control_element)) {
      control_element.SetAutofillValue(blink::WebString());
    } else if (form_util::IsSelectElement(control_element)) {
      WebSelectElement select_element = control_element.To<WebSelectElement>();

      std::map<const WebSelectElement, base::string16>::const_iterator
          initial_value_iter = initial_select_values_.find(select_element);
      if (initial_value_iter != initial_select_values_.end() &&
          select_element.Value().Utf16() != initial_value_iter->second) {
        select_element.SetAutofillValue(
            blink::WebString::FromUTF16(initial_value_iter->second));
      }
    } else {
      WebInputElement input_element = control_element.To<WebInputElement>();
      DCHECK(form_util::IsCheckableElement(&input_element));
      std::map<const WebInputElement, bool>::const_iterator it =
          initial_checked_state_.find(input_element);
      if (it != initial_checked_state_.end() &&
          input_element.IsChecked() != it->second) {
        input_element.SetChecked(it->second, true);
      }
    }
  }

  return true;
}

bool FormCache::ShowPredictions(const FormDataPredictions& form) {
  DCHECK_EQ(form.data.fields.size(), form.fields.size());

  std::vector<WebFormControlElement> control_elements;

  // First check the synthetic form.
  bool found_synthetic_form = false;
  if (form.data.SameFormAs(synthetic_form_)) {
    found_synthetic_form = true;
    WebDocument document = frame_.GetDocument();
    control_elements = form_util::GetUnownedAutofillableFormFieldElements(
        document.All(), nullptr);
  }

  if (!found_synthetic_form) {
    // Find the real form by searching through the WebDocuments.
    bool found_form = false;
    WebFormElement form_element;
    WebVector<WebFormElement> web_forms;
    frame_.GetDocument().Forms(web_forms);

    for (size_t i = 0; i < web_forms.size(); ++i) {
      form_element = web_forms[i];
      // Note: matching on the form name here which is not guaranteed to be
      // unique for the page, nor is it guaranteed to be non-empty.  Ideally,
      // we would have a way to uniquely identify the form cross-process. For
      // now, we'll check form name and form action for identity.
      // Also note that WebString() == WebString(string16()) does not evaluate
      // to |true| -- WebKit distinguishes between a "null" string (lhs) and
      // an "empty" string (rhs). We don't want that distinction, so forcing
      // to string16.
      base::string16 element_name = form_util::GetFormIdentifier(form_element);
      GURL action(
          form_element.GetDocument().CompleteURL(form_element.Action()));
      if (element_name == form.data.name && action == form.data.action) {
        found_form = true;
        control_elements =
            form_util::ExtractAutofillableElementsInForm(form_element);
        break;
      }
    }

    if (!found_form)
      return false;
  }

  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement& element = control_elements[i];

    const FormFieldData& field_data = form.data.fields[i];
    if (element.NameForAutofill().Utf16() != field_data.name) {
      // Keep things simple.  Don't show predictions for elements whose names
      // were modified between page load and the server's response to our query.
      continue;
    }

    static const size_t kMaxLabelSize = 100;
    const base::string16 truncated_label = field_data.label.substr(
        0, std::min(field_data.label.length(), kMaxLabelSize));

    const FormFieldDataPredictions& field = form.fields[i];
    std::vector<base::string16> replacements;

    base::string16 overall_type = base::UTF8ToUTF16(field.overall_type);

    replacements.push_back(overall_type);
    replacements.push_back(base::UTF8ToUTF16(field.server_type));
    replacements.push_back(base::UTF8ToUTF16(field.heuristic_type));
    replacements.push_back(truncated_label);
    replacements.push_back(base::UTF8ToUTF16(field.parseable_name));
    replacements.push_back(base::UTF8ToUTF16(field.signature));
    replacements.push_back(base::UTF8ToUTF16(form.signature));
    const base::string16 title = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SHOW_PREDICTIONS_TITLE, replacements, nullptr);
    element.SetAttribute("title", WebString::FromUTF16(title));

    element.SetAttribute("autofill-prediction",
                         WebString::FromUTF16(overall_type));
  }

  return true;
}

size_t FormCache::ScanFormControlElements(
    const std::vector<WebFormControlElement>& control_elements,
    bool log_deprecation_messages) {
  size_t num_editable_elements = 0;
  for (size_t i = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& element = control_elements[i];

    if (log_deprecation_messages)
      LogDeprecationMessages(element);

    // Save original values of <select> elements so we can restore them
    // when |ClearFormWithNode()| is invoked.
    if (form_util::IsSelectElement(element) ||
        form_util::IsTextAreaElement(element)) {
      ++num_editable_elements;
    } else {
      const WebInputElement input_element = element.ToConst<WebInputElement>();
      if (!form_util::IsCheckableElement(&input_element))
        ++num_editable_elements;
    }
  }
  return num_editable_elements;
}

void FormCache::SaveInitialValues(
    const std::vector<WebFormControlElement>& control_elements) {
  for (const WebFormControlElement& element : control_elements) {
    if (form_util::IsSelectElement(element)) {
      const WebSelectElement select_element =
          element.ToConst<WebSelectElement>();
      initial_select_values_.insert(
          std::make_pair(select_element, select_element.Value().Utf16()));
    } else {
      const WebInputElement* input_element = ToWebInputElement(&element);
      if (form_util::IsCheckableElement(input_element)) {
        initial_checked_state_.insert(
            std::make_pair(*input_element, input_element->IsChecked()));
      }
    }
  }
}

}  // namespace autofill
