// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/form_cache.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_data_predictions.h"
#include "chrome/common/form_field_data.h"
#include "chrome/common/form_field_data_predictions.h"
#include "chrome/renderer/autofill/form_autofill_util.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSelectElement.h"
#include "ui/base/l10n/l10n_util.h"

using WebKit::WebDocument;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebSelectElement;
using WebKit::WebString;
using WebKit::WebVector;

namespace {

// The number of fields required by Autofill.  Ideally we could send the forms
// to Autofill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
const size_t kRequiredAutofillFields = 3;

}  // namespace

namespace autofill {

FormCache::FormCache() {
}

FormCache::~FormCache() {
}

void FormCache::ExtractForms(const WebFrame& frame,
                             std::vector<FormData>* forms) {
  // Reset the cache for this frame.
  ResetFrame(frame);

  WebDocument document = frame.document();
  if (document.isNull())
    return;

  web_documents_.insert(document);

  WebVector<WebFormElement> web_forms;
  document.forms(web_forms);

  size_t num_fields_seen = 0;
  for (size_t i = 0; i < web_forms.size(); ++i) {
    WebFormElement form_element = web_forms[i];

    std::vector<WebFormControlElement> control_elements;
    ExtractAutofillableElements(form_element, autofill::REQUIRE_NONE,
                                &control_elements);
    for (size_t j = 0; j < control_elements.size(); ++j) {
      WebFormControlElement element = control_elements[j];

      // Save original values of <select> elements so we can restore them
      // when |ClearFormWithNode()| is invoked.
      if (IsSelectElement(element)) {
        const WebSelectElement select_element =
            element.toConst<WebSelectElement>();
        initial_select_values_.insert(std::make_pair(select_element,
                                                     select_element.value()));
      }
    }

    // To avoid overly expensive computation, we impose a minimum number of
    // allowable fields.  The corresponding maximum number of allowable fields
    // is imposed by WebFormElementToFormData().
    if (control_elements.size() < kRequiredAutofillFields)
      continue;

    FormData form;
    if (!WebFormElementToFormData(form_element, WebFormControlElement(),
                                  REQUIRE_NONE, EXTRACT_VALUE, &form, NULL)) {
      continue;
    }

    num_fields_seen += form.fields.size();
    if (num_fields_seen > kMaxParseableFields)
      break;

    if (form.fields.size() >= kRequiredAutofillFields)
      forms->push_back(form);
  }
}

void FormCache::ResetFrame(const WebFrame& frame) {
  std::vector<WebDocument> documents_to_delete;
  for (std::set<WebDocument>::const_iterator it = web_documents_.begin();
       it != web_documents_.end(); ++it) {
    const WebFrame* document_frame = it->frame();
    if (!document_frame || document_frame == &frame)
      documents_to_delete.push_back(*it);
  }

  for (std::vector<WebDocument>::const_iterator it =
           documents_to_delete.begin();
       it != documents_to_delete.end(); ++it) {
    web_documents_.erase(*it);
  }

  std::vector<WebSelectElement> select_values_to_delete;
  for (std::map<const WebSelectElement, string16>::const_iterator it =
           initial_select_values_.begin();
       it != initial_select_values_.end(); ++it) {
    WebFormElement form_element = it->first.form();
    if (form_element.isNull()) {
      select_values_to_delete.push_back(it->first);
    } else {
      const WebFrame* element_frame = form_element.document().frame();
      if (!element_frame || element_frame == &frame)
        select_values_to_delete.push_back(it->first);
    }
  }

  for (std::vector<WebSelectElement>::const_iterator it =
           select_values_to_delete.begin();
       it != select_values_to_delete.end(); ++it) {
    initial_select_values_.erase(*it);
  }
}

bool FormCache::ClearFormWithElement(const WebInputElement& element) {
  WebFormElement form_element = element.form();
  if (form_element.isNull())
    return false;

  std::vector<WebFormControlElement> control_elements;
  ExtractAutofillableElements(form_element, autofill::REQUIRE_NONE,
                              &control_elements);
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement control_element = control_elements[i];
    WebInputElement* input_element = toWebInputElement(&control_element);
    if (IsTextInput(input_element)) {
      // We don't modify the value of disabled fields.
      if (!input_element->isEnabled())
        continue;

      input_element->setValue(string16(), true);
      input_element->setAutofilled(false);

      // Clearing the value in the focused node (above) can cause selection
      // to be lost. We force selection range to restore the text cursor.
      if (element == *input_element) {
        int length = input_element->value().length();
        input_element->setSelectionRange(length, length);
      }
    } else {
      DCHECK(IsSelectElement(control_element));
      WebSelectElement select_element = control_element.to<WebSelectElement>();

      std::map<const WebSelectElement, string16>::const_iterator
          initial_value_iter = initial_select_values_.find(select_element);
      if (initial_value_iter != initial_select_values_.end() &&
          select_element.value() != initial_value_iter->second) {
        select_element.setValue(initial_value_iter->second);
        select_element.dispatchFormControlChangeEvent();
      }
    }
  }

  return true;
}

bool FormCache::ShowPredictions(const FormDataPredictions& form) {
  DCHECK_EQ(form.data.fields.size(), form.fields.size());

  // Find the form.
  bool found_form = false;
  WebFormElement form_element;
  for (std::set<WebDocument>::const_iterator it = web_documents_.begin();
       it != web_documents_.end() && !found_form; ++it) {
    WebVector<WebFormElement> web_forms;
    it->forms(web_forms);

    for (size_t i = 0; i < web_forms.size(); ++i) {
      form_element = web_forms[i];

      // Note: matching on the form name here which is not guaranteed to be
      // unique for the page, nor is it guaranteed to be non-empty.  Ideally, we
      // would have a way to uniquely identify the form cross-process.  For now,
      // we'll check form name and form action for identity.
      // Also note that WebString() == WebString(string16()) does not evaluate
      // to |true| -- WebKit distinguishes between a "null" string (lhs) and an
      // "empty" string (rhs).  We don't want that distinction, so forcing to
      // string16.
      string16 element_name = GetFormIdentifier(form_element);
      GURL action(form_element.document().completeURL(form_element.action()));
      if (element_name == form.data.name && action == form.data.action) {
        found_form = true;
        break;
      }
    }
  }

  if (!found_form)
    return false;

  std::vector<WebFormControlElement> control_elements;
  ExtractAutofillableElements(form_element, autofill::REQUIRE_NONE,
                              &control_elements);
  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement* element = &control_elements[i];

    if (string16(element->nameForAutofill()) != form.data.fields[i].name) {
      // Keep things simple.  Don't show predictions for elements whose names
      // were modified between page load and the server's response to our query.
      continue;
    }

    std::string placeholder = form.fields[i].overall_type;
    string16 title = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SHOW_PREDICTIONS_TITLE,
        UTF8ToUTF16(form.fields[i].heuristic_type),
        UTF8ToUTF16(form.fields[i].server_type),
        UTF8ToUTF16(form.fields[i].signature),
        UTF8ToUTF16(form.signature),
        UTF8ToUTF16(form.experiment_id));
    if (!element->hasAttribute("placeholder"))
      element->setAttribute("placeholder", WebString(UTF8ToUTF16(placeholder)));
    element->setAttribute("title", WebString(title));
  }

  return true;
}

}  // namespace autofill
