// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/form_manager.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stl_util-inl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebLabelElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSelectElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebLabelElement;
using WebKit::WebNode;
using WebKit::WebNodeList;
using WebKit::WebSelectElement;
using WebKit::WebString;
using WebKit::WebVector;

namespace {

// The number of fields required by AutoFill.  Ideally we could send the forms
// to AutoFill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
const size_t kRequiredAutoFillFields = 3;

}  // namespace

FormManager::FormManager() {
}

FormManager::~FormManager() {
  Reset();
}

// static
void FormManager::WebFormControlElementToFormField(
    const WebFormControlElement& element, FormField* field) {
  DCHECK(field);

  field->set_label(LabelForElement(element));
  field->set_name(element.nameForAutofill());
  field->set_form_control_type(element.formControlType());

  // TODO(jhawkins): In WebKit, move value() and setValue() to
  // WebFormControlElement.
  string16 value;
  if (element.formControlType() == ASCIIToUTF16("text")) {
    const WebInputElement& input_element =
        element.toConstElement<WebInputElement>();
    value = input_element.value();
  } else if (element.formControlType() == ASCIIToUTF16("select-one")) {
    // TODO(jhawkins): This is ugly.  WebSelectElement::value() is a non-const
    // method.  Look into fixing this on the WebKit side.
    WebFormControlElement& e = const_cast<WebFormControlElement&>(element);
    WebSelectElement select_element = e.toElement<WebSelectElement>();
    value = select_element.value();
  }
  field->set_value(value);
}

void FormManager::ExtractForms(const WebFrame* frame) {
  DCHECK(frame);

  // Reset the vector of FormElements for this frame.
  ResetFrame(frame);

  WebVector<WebFormElement> web_forms;
  frame->forms(web_forms);

  for (size_t i = 0; i < web_forms.size(); ++i) {
    FormElement* form_elements = new FormElement;
    form_elements->form_element = web_forms[i];

    WebVector<WebFormControlElement> control_elements;
    form_elements->form_element.getFormControlElements(control_elements);
    for (size_t j = 0; j < control_elements.size(); ++j) {
      WebFormControlElement element = control_elements[j];
      // TODO(jhawkins): Remove this check when we have labels.
      if (!element.nameForAutofill().isEmpty())
        form_elements->control_elements[element.nameForAutofill()] = element;
    }

    form_elements_map_[frame].push_back(form_elements);
  }
}

void FormManager::GetForms(RequirementsMask requirements,
                           std::vector<FormData>* forms) {
  DCHECK(forms);

  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    const WebFrame* frame = iter->first;

    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      FormElement* form_element = *form_iter;

      if (requirements & REQUIRE_AUTOCOMPLETE &&
          !form_element->form_element.autoComplete())
        continue;

      FormData form;
      FormElementToFormData(frame, form_element, requirements, &form);
      forms->push_back(form);
    }
  }
}

void FormManager::GetFormsInFrame(const WebFrame* frame,
                                  RequirementsMask requirements,
                                  std::vector<FormData>* forms) {
  DCHECK(frame);
  DCHECK(forms);

  WebFrameFormElementMap::iterator iter = form_elements_map_.find(frame);
  if (iter == form_elements_map_.end())
    return;

  // TODO(jhawkins): Factor this out and use it here and in GetForms.
  const std::vector<FormElement*>& form_elements = iter->second;
  for (std::vector<FormElement*>::const_iterator form_iter =
           form_elements.begin();
       form_iter != form_elements.end(); ++form_iter) {
    FormElement* form_element = *form_iter;

    // We need at least |kRequiredAutoFillFields| fields before appending this
    // form to |forms|.
    if (form_element->control_elements.size() < kRequiredAutoFillFields)
      continue;

    if (requirements & REQUIRE_AUTOCOMPLETE &&
        !form_element->form_element.autoComplete())
      continue;

    FormData form;
    FormElementToFormData(frame, form_element, requirements, &form);
    if (form.fields.size() >= kRequiredAutoFillFields)
      forms->push_back(form);
  }
}

bool FormManager::FindForm(const WebFormElement& element,
                           RequirementsMask requirements,
                           FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.frame();
  if (!frame)
    return false;

  WebFrameFormElementMap::const_iterator frame_iter =
      form_elements_map_.find(frame);
  if (frame_iter == form_elements_map_.end())
    return false;

  for (std::vector<FormElement*>::const_iterator iter =
           frame_iter->second.begin();
       iter != frame_iter->second.end(); ++iter) {
    if ((*iter)->form_element.name() != element.name())
      continue;

    return FormElementToFormData(frame, *iter, requirements, form);
  }

  return false;
}

bool FormManager::FindFormWithFormControlElement(
    const WebFormControlElement& element,
    RequirementsMask requirements,
    FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.frame();
  if (!frame)
    return false;

  if (form_elements_map_.find(frame) == form_elements_map_.end())
    return false;

  const std::vector<FormElement*> forms = form_elements_map_[frame];
  for (std::vector<FormElement*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    const FormElement* form_element = *iter;

    if (form_element->control_elements.find(element.nameForAutofill()) !=
        form_element->control_elements.end()) {
      FormElementToFormData(frame, form_element, requirements, form);
      return true;
    }
  }

  return false;
}

bool FormManager::FillForm(const FormData& form) {
  FormElement* form_element = NULL;

  // Frame loop.
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    // Form loop.
    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      // TODO(dhollowa): matching on form name here which is not guaranteed to
      // be unique for the page, nor is it guaranteed to be non-empty.  Need to
      // find a way to uniquely identify the form cross-process.
      // http://crbug.com/37990 test file sample8.html.
      // Also note that WebString() == WebString(string16()) does not seem to
      // evaluate to |true| for some reason TBD, so forcing to string16.
      string16 element_name((*form_iter)->form_element.name());
      if (element_name == form.name &&
          (*form_iter)->control_elements.size() == form.fields.size()) {
        form_element = *form_iter;
        break;
      }
    }
  }

  if (!form_element)
    return false;

  DCHECK(form_element->control_elements.size() == form.fields.size());

  size_t i = 0;
  for (FormControlElementMap::iterator iter =
           form_element->control_elements.begin();
      iter != form_element->control_elements.end(); ++iter, ++i) {
    DCHECK_EQ(form.fields[i].name(), iter->second.nameForAutofill());

    if (!form.fields[i].value().empty() &&
        iter->second.formControlType() != ASCIIToUTF16("submit")) {
      if (iter->second.formControlType() == ASCIIToUTF16("text")) {
        WebInputElement input_element =
            iter->second.toElement<WebInputElement>();
        input_element.setValue(form.fields[i].value());
        input_element.setAutofilled(true);
      } else if (iter->second.formControlType() == ASCIIToUTF16("select-one")) {
        WebSelectElement select_element =
            iter->second.toElement<WebSelectElement>();
        select_element.setValue(form.fields[i].value());
      }
    }
  }

  return true;
}

void FormManager::Reset() {
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    STLDeleteElements(&iter->second);
  }
  form_elements_map_.clear();
}

// static
bool FormManager::FormElementToFormData(const WebFrame* frame,
                                        const FormElement* form_element,
                                        RequirementsMask requirements,
                                        FormData* form) {
  if (requirements & REQUIRE_AUTOCOMPLETE &&
      !form_element->form_element.autoComplete())
    return false;

  form->name = form_element->form_element.name();
  form->method = form_element->form_element.method();
  form->origin = frame->url();
  form->action = frame->completeURL(form_element->form_element.action());

  // If the completed ULR is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(form_element->form_element.action());

  // Form elements loop.
  for (FormControlElementMap::const_iterator element_iter =
           form_element->control_elements.begin();
       element_iter != form_element->control_elements.end(); ++element_iter) {
    WebFormControlElement control_element = element_iter->second;

    if (requirements & REQUIRE_AUTOCOMPLETE &&
        control_element.formControlType() == ASCIIToUTF16("text")) {
      const WebInputElement& input_element =
          control_element.toConstElement<WebInputElement>();
      if (!input_element.autoComplete())
        continue;
    }

    if (requirements & REQUIRE_ELEMENTS_ENABLED && !control_element.isEnabled())
      continue;

    FormField field;
    WebFormControlElementToFormField(control_element, &field);
    form->fields.push_back(field);
  }

  return true;
}

void FormManager::ResetFrame(const WebFrame* frame) {
  WebFrameFormElementMap::iterator iter = form_elements_map_.find(frame);
  if (iter != form_elements_map_.end()) {
    STLDeleteElements(&iter->second);
    form_elements_map_.erase(iter);
  }
}

// static
string16 FormManager::LabelForElement(const WebFormControlElement& element) {
  WebNodeList labels = element.document().getElementsByTagName("label");
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebElement e = labels.item(i).toElement<WebElement>();
    if (e.hasTagName("label")) {
      WebLabelElement label = e.toElement<WebLabelElement>();
      if (label.correspondingControl() == element)
        return label.innerText();
    }
  }

  // Infer the label from context if not found in label element.
  return FormManager::InferLabelForElement(element);
}

// static
string16 FormManager::InferLabelForElement(
    const WebFormControlElement& element) {
  string16 inferred_label;
  WebNode previous = element.previousSibling();
  if (!previous.isNull()) {
    if (previous.isTextNode()) {
      inferred_label = previous.nodeValue();
      TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
    }

    // If we didn't find text, check for previous paragraph.
    // Eg. <p>Some Text</p><input ...>
    // Note the lack of whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
      if (previous.isElementNode()) {
        WebElement element = previous.toElement<WebElement>();
        if (element.hasTagName("p")) {
          inferred_label = element.innerText();
          TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
        }
      }
    }

    // If we didn't find paragraph, check for previous paragraph to this.
    // Eg. <p>Some Text</p>   <input ...>
    // Note the whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
      previous = previous.previousSibling();
      if (!previous.isNull() && previous.isElementNode()) {
        WebElement element = previous.toElement<WebElement>();
        if (element.hasTagName("p")) {
          inferred_label = element.innerText();
          TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
        }
      }
    }
  }

  // If we didn't find paragraph, check for table cell case.
  // Eg. <tr><td>Some Text</td><td><input ...></td></tr>
  if (inferred_label.empty()) {
    WebNode parent = element.parentNode();
    if (!parent.isNull() && parent.isElementNode()) {
      WebElement element = parent.toElement<WebElement>();
      if (element.hasTagName("td")) {
        previous = parent.previousSibling();

        // Skip by any intervening text nodes.
        while (!previous.isNull() && previous.isTextNode())
          previous = previous.previousSibling();

        if (!previous.isNull() && previous.isElementNode()) {
          element = previous.toElement<WebElement>();
          if (element.hasTagName("td")) {
            inferred_label = element.innerText();
            TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
          }
        }
      }
    }
  }

  return inferred_label;
}
