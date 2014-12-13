// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "grit/components_strings.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebSelectElement.h"
#include "third_party/WebKit/public/web/WebTextAreaElement.h"
#include "ui/base/l10n/l10n_util.h"

using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebTextAreaElement;
using blink::WebVector;

namespace autofill {

namespace {

// Helper function to discard state of various WebFormElements when they go out
// of web frame's scope. This is done to release memory that we no longer need
// to hold.
// K should inherit from WebFormControlElement as the function looks to extract
// WebFormElement for K.form().
template <class K, class V>
void RemoveOldElements(const WebFrame& frame, std::map<const K, V>* states) {
  std::vector<K> to_remove;
  for (typename std::map<const K, V>::const_iterator it = states->begin();
       it != states->end(); ++it) {
    const WebFormElement& form_element = it->first.form();
    if (form_element.isNull()) {
      to_remove.push_back(it->first);
    } else {
      const WebFrame* element_frame = form_element.document().frame();
      if (!element_frame || element_frame == &frame)
        to_remove.push_back(it->first);
    }
  }

  for (typename std::vector<K>::const_iterator it = to_remove.begin();
       it != to_remove.end(); ++it) {
    states->erase(*it);
  }
}

void LogDeprecationMessages(const WebFormControlElement& element) {
  std::string autocomplete_attribute =
      base::UTF16ToUTF8(element.getAttribute("autocomplete"));

  static const char* const deprecated[] = { "region", "locality" };
  for (size_t i = 0; i < arraysize(deprecated); ++i) {
    if (autocomplete_attribute.find(deprecated[i]) == std::string::npos)
      continue;
    std::string msg = std::string("autocomplete='") + deprecated[i] +
        "' is deprecated and will soon be ignored. See http://goo.gl/YjeSsW";
    WebConsoleMessage console_message = WebConsoleMessage(
        WebConsoleMessage::LevelWarning,
        WebString(base::ASCIIToUTF16(msg)));
    element.document().frame()->addMessageToConsole(console_message);
  }
}

// To avoid overly expensive computation, we impose a minimum number of
// allowable fields.  The corresponding maximum number of allowable fields
// is imposed by WebFormElementToFormData().
bool ShouldIgnoreForm(size_t num_editable_elements,
                      size_t num_control_elements) {
  return (num_editable_elements < kRequiredAutofillFields &&
          num_control_elements > 0);
}

}  // namespace

FormCache::FormCache() {
}

FormCache::~FormCache() {
}

std::vector<FormData> FormCache::ExtractNewForms(const WebFrame& frame) {
  std::vector<FormData> forms;
  WebDocument document = frame.document();
  if (document.isNull())
    return forms;

  if (!ContainsKey(documents_to_synthetic_form_map_, document))
    documents_to_synthetic_form_map_[document] = FormData();

  WebVector<WebFormElement> web_forms;
  document.forms(web_forms);

  // Log an error message for deprecated attributes, but only the first time
  // the form is parsed.
  bool log_deprecation_messages = !ContainsKey(parsed_forms_, &frame);

  const ExtractMask extract_mask =
      static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS);

  size_t num_fields_seen = 0;
  for (size_t i = 0; i < web_forms.size(); ++i) {
    const WebFormElement& form_element = web_forms[i];

    std::vector<WebFormControlElement> control_elements =
        ExtractAutofillableElementsInForm(form_element, REQUIRE_NONE);
    size_t num_editable_elements =
        ScanFormControlElements(control_elements, log_deprecation_messages);

    if (ShouldIgnoreForm(num_editable_elements, control_elements.size()))
      continue;

    FormData form;
    if (!WebFormElementToFormData(form_element, WebFormControlElement(),
                                  REQUIRE_NONE, extract_mask, &form, nullptr)) {
      continue;
    }

    num_fields_seen += form.fields.size();
    if (num_fields_seen > kMaxParseableFields)
      return forms;

    if (form.fields.size() >= kRequiredAutofillFields &&
        !ContainsKey(parsed_forms_[&frame], form)) {
      forms.push_back(form);
      parsed_forms_[&frame].insert(form);
    }
  }

  // Look for more parseable fields outside of forms.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      GetUnownedAutofillableFormFieldElements(document.all(), &fieldsets);

  size_t num_editable_elements =
      ScanFormControlElements(control_elements, log_deprecation_messages);

  if (ShouldIgnoreForm(num_editable_elements, control_elements.size()))
    return forms;

  FormData form;
  if (!UnownedFormElementsAndFieldSetsToFormData(fieldsets, control_elements,
                                                 nullptr, document.url(),
                                                 REQUIRE_NONE, extract_mask,
                                                 &form, nullptr)) {
    return forms;
  }

  num_fields_seen += form.fields.size();
  if (num_fields_seen > kMaxParseableFields)
    return forms;

  if (form.fields.size() >= kRequiredAutofillFields &&
      !parsed_forms_[&frame].count(form)) {
    forms.push_back(form);
    parsed_forms_[&frame].insert(form);
    documents_to_synthetic_form_map_[document] = form;
  }
  return forms;
}

void FormCache::ResetFrame(const WebFrame& frame) {
  std::vector<WebDocument> documents_to_delete;
  for (const auto& it : documents_to_synthetic_form_map_) {
    const WebFrame* document_frame = it.first.frame();
    if (!document_frame || document_frame == &frame)
      documents_to_delete.push_back(it.first);
  }

  for (const auto& it : documents_to_delete)
    documents_to_synthetic_form_map_.erase(it);

  parsed_forms_[&frame].clear();
  RemoveOldElements(frame, &initial_select_values_);
  RemoveOldElements(frame, &initial_checked_state_);
}

bool FormCache::ClearFormWithElement(const WebFormControlElement& element) {
  WebFormElement form_element = element.form();
  std::vector<WebFormControlElement> control_elements;
  if (form_element.isNull()) {
    control_elements = GetUnownedAutofillableFormFieldElements(
        element.document().all(), nullptr);
  } else {
    control_elements = ExtractAutofillableElementsInForm(
        form_element, REQUIRE_NONE);
  }
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement control_element = control_elements[i];
    // Don't modify the value of disabled fields.
    if (!control_element.isEnabled())
      continue;

    // Don't clear field that was not autofilled
    if (!control_element.isAutofilled())
      continue;

    control_element.setAutofilled(false);

    WebInputElement* input_element = toWebInputElement(&control_element);
    if (IsTextInput(input_element) || IsMonthInput(input_element)) {
      input_element->setValue(base::string16(), true);

      // Clearing the value in the focused node (above) can cause selection
      // to be lost. We force selection range to restore the text cursor.
      if (element == *input_element) {
        int length = input_element->value().length();
        input_element->setSelectionRange(length, length);
      }
    } else if (IsTextAreaElement(control_element)) {
      control_element.setValue(base::string16(), true);
    } else if (IsSelectElement(control_element)) {
      WebSelectElement select_element = control_element.to<WebSelectElement>();

      std::map<const WebSelectElement, base::string16>::const_iterator
          initial_value_iter = initial_select_values_.find(select_element);
      if (initial_value_iter != initial_select_values_.end() &&
          select_element.value() != initial_value_iter->second) {
        select_element.setValue(initial_value_iter->second, true);
      }
    } else {
      WebInputElement input_element = control_element.to<WebInputElement>();
      DCHECK(IsCheckableElement(&input_element));
      std::map<const WebInputElement, bool>::const_iterator it =
          initial_checked_state_.find(input_element);
      if (it != initial_checked_state_.end() &&
          input_element.isChecked() != it->second) {
        input_element.setChecked(it->second, true);
      }
    }
  }

  return true;
}

bool FormCache::ShowPredictions(const FormDataPredictions& form) {
  DCHECK_EQ(form.data.fields.size(), form.fields.size());

  std::vector<WebFormControlElement> control_elements;

  // First check the synthetic forms.
  bool found_synthetic_form = false;
  for (const auto& it : documents_to_synthetic_form_map_) {
    const FormData& form_data = it.second;
    if (!form_data.SameFormAs(form.data))
      continue;

    found_synthetic_form = true;
    WebDocument document = it.first;
    control_elements =
        GetUnownedAutofillableFormFieldElements(document.all(), nullptr);
    break;
  }

  if (!found_synthetic_form) {
    // Find the real form by searching through the WebDocuments.
    bool found_form = false;
    WebFormElement form_element;
    for (const auto& it : documents_to_synthetic_form_map_) {
      WebVector<WebFormElement> web_forms;
      it.first.forms(web_forms);

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
        base::string16 element_name = GetFormIdentifier(form_element);
        GURL action(form_element.document().completeURL(form_element.action()));
        if (element_name == form.data.name && action == form.data.action) {
          found_form = true;
          control_elements =
              ExtractAutofillableElementsInForm(form_element, REQUIRE_NONE);
          break;
        }
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

    if (base::string16(element.nameForAutofill()) != form.data.fields[i].name) {
      // Keep things simple.  Don't show predictions for elements whose names
      // were modified between page load and the server's response to our query.
      continue;
    }

    std::string placeholder = form.fields[i].overall_type;
    base::string16 title = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SHOW_PREDICTIONS_TITLE,
        base::UTF8ToUTF16(form.fields[i].heuristic_type),
        base::UTF8ToUTF16(form.fields[i].server_type),
        base::UTF8ToUTF16(form.fields[i].signature),
        base::UTF8ToUTF16(form.signature),
        base::UTF8ToUTF16(form.experiment_id));
    if (!element.hasAttribute("placeholder")) {
      element.setAttribute("placeholder",
                           WebString(base::UTF8ToUTF16(placeholder)));
    }
    element.setAttribute("title", WebString(title));
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
    if (IsSelectElement(element)) {
      const WebSelectElement select_element =
          element.toConst<WebSelectElement>();
      initial_select_values_.insert(
          std::make_pair(select_element, select_element.value()));
      ++num_editable_elements;
    } else if (IsTextAreaElement(element)) {
      ++num_editable_elements;
    } else {
      const WebInputElement input_element =
          element.toConst<WebInputElement>();
      if (IsCheckableElement(&input_element)) {
        initial_checked_state_.insert(
            std::make_pair(input_element, input_element.isChecked()));
      } else {
        ++num_editable_elements;
      }
    }
  }
  return num_editable_elements;
}

}  // namespace autofill
