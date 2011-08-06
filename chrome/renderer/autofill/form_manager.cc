// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/form_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebLabelElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebOptionElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSelectElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_data_predictions.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/form_field_predictions.h"
#include "webkit/glue/web_io_operators.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebLabelElement;
using WebKit::WebNode;
using WebKit::WebNodeList;
using WebKit::WebOptionElement;
using WebKit::WebSelectElement;
using WebKit::WebString;
using WebKit::WebVector;
using webkit_glue::FormData;
using webkit_glue::FormDataPredictions;
using webkit_glue::FormField;
using webkit_glue::FormFieldPredictions;

namespace {

// The number of fields required by Autofill.  Ideally we could send the forms
// to Autofill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
const size_t kRequiredAutofillFields = 3;

// The maximum number of form fields we are willing to parse, due to
// computational costs.  Several examples of forms with lots of fields that are
// not relevant to Autofill: (1) the Netflix queue; (2) the Amazon wishlist;
// (3) router configuration pages; and (4) other configuration pages, e.g. for
// Google code project settings.
const size_t kMaxParseableFields = 100;

// The maximum length allowed for form data.
const size_t kMaxDataLength = 1024;

// In HTML5, all text fields except password are text input fields to
// autocomplete.
bool IsTextInput(const WebInputElement* element) {
  if (!element)
    return false;

  return element->isTextField() && !element->isPasswordField();
}

bool IsSelectElement(const WebFormControlElement& element) {
  return element.formControlType() == ASCIIToUTF16("select-one");
}

bool IsOptionElement(const WebElement& element) {
  return element.hasTagName("option");
}

bool IsScriptElement(const WebElement& element) {
  return element.hasTagName("script");
}

bool IsNoScriptElement(const WebElement& element) {
  return element.hasTagName("noscript");
}

bool HasTagName(const WebNode& node, const WebKit::WebString& tag) {
  return node.isElementNode() && node.toConst<WebElement>().hasTagName(tag);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement* input_element = toWebInputElement(&element);
  return IsTextInput(input_element) || IsSelectElement(element);
}

// Appends |suffix| to |prefix| so that any intermediary whitespace is collapsed
// to a single space.  If |force_whitespace| is true, then the resulting string
// is guaranteed to have a space between |prefix| and |suffix|.  Otherwise, the
// result includes a space only if |prefix| has trailing whitespace or |suffix|
// has leading whitespace.
// A few examples:
//  * CombineAndCollapseWhitespace("foo", "bar", false)       -> "foobar"
//  * CombineAndCollapseWhitespace("foo", "bar", true)        -> "foo bar"
//  * CombineAndCollapseWhitespace("foo ", "bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", true)       -> "foo bar"
//  * CombineAndCollapseWhitespace("foo   ", "   bar", false) -> "foo bar"
//  * CombineAndCollapseWhitespace(" foo", "bar ", false)     -> " foobar "
//  * CombineAndCollapseWhitespace(" foo", "bar ", true)      -> " foo bar "
const string16 CombineAndCollapseWhitespace(const string16& prefix,
                                            const string16& suffix,
                                            bool force_whitespace) {
  string16 prefix_trimmed;
  TrimPositions prefix_trailing_whitespace =
      TrimWhitespace(prefix, TRIM_TRAILING, &prefix_trimmed);

  // Recursively compute the children's text.
  string16 suffix_trimmed;
  TrimPositions suffix_leading_whitespace =
      TrimWhitespace(suffix, TRIM_LEADING, &suffix_trimmed);

  if (prefix_trailing_whitespace || suffix_leading_whitespace ||
      force_whitespace) {
    return prefix_trimmed + ASCIIToUTF16(" ") + suffix_trimmed;
  } else {
    return prefix_trimmed + suffix_trimmed;
  }
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
string16 FindChildTextInner(const WebNode& node, int depth) {
  if (depth <= 0 || node.isNull())
    return string16();

  // Skip over comments.
  if (node.nodeType() == WebNode::CommentNode)
    return FindChildTextInner(node.nextSibling(), depth - 1);

  if (node.nodeType() != WebNode::ElementNode &&
      node.nodeType() != WebNode::TextNode)
    return string16();

  // Ignore elements known not to contain inferable labels.
  if (node.isElementNode()) {
    const WebElement element = node.toConst<WebElement>();
    if (IsOptionElement(element) ||
        IsScriptElement(element) ||
        IsNoScriptElement(element) ||
        (element.isFormControlElement() &&
         IsAutofillableElement(element.toConst<WebFormControlElement>()))) {
      return string16();
    }
  }

  // Extract the text exactly at this node.
  string16 node_text = node.nodeValue();

  // Recursively compute the children's text.
  // Preserve inter-element whitespace separation.
  string16 child_text = FindChildTextInner(node.firstChild(), depth - 1);
  bool add_space = node.nodeType() == WebNode::TextNode && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, child_text, add_space);

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  string16 sibling_text = FindChildTextInner(node.nextSibling(), depth - 1);
  add_space = node.nodeType() == WebNode::TextNode && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, sibling_text, add_space);

  return node_text;
}

// Returns the aggregated values of the descendants of |element| that are
// non-empty text nodes.  This is a faster alternative to |innerText()| for
// performance critical operations.  It does a full depth-first search so can be
// used when the structure is not directly known.  However, unlike with
// |innerText()|, the search depth and breadth are limited to a fixed threshold.
// Whitespace is trimmed from text accumulated at descendant nodes.
string16 FindChildText(const WebNode& node) {
  if (node.isTextNode())
    return node.nodeValue();

  WebNode child = node.firstChild();

  const int kChildSearchDepth = 10;
  string16 node_text = FindChildTextInner(child, kChildSearchDepth);
  TrimWhitespace(node_text, TRIM_ALL, &node_text);
  return node_text;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous sibling of |element|,
// e.g. Some Text <input ...>
// or   Some <span>Text</span> <input ...>
// or   <p>Some Text</p><input ...>
// or   <label>Some Text</label> <input ...>
// or   Some Text <img><input ...>
// or   <b>Some Text</b><br/> <input ...>.
string16 InferLabelFromPrevious(const WebFormControlElement& element) {
  string16 inferred_label;
  WebNode previous = element;
  while (true) {
    previous = previous.previousSibling();
    if (previous.isNull())
      break;

    // Skip over comments.
    WebNode::NodeType node_type = previous.nodeType();
    if (node_type == WebNode::CommentNode)
      continue;

    // Otherwise, only consider normal HTML elements and their contents.
    if (node_type != WebNode::TextNode &&
        node_type != WebNode::ElementNode)
      break;

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    if (previous.isTextNode() ||
        HasTagName(previous, "b") || HasTagName(previous, "strong") ||
        HasTagName(previous, "span") || HasTagName(previous, "font")) {
      string16 value = FindChildText(previous);
      // A text node's value will be empty if it is for a line break.
      bool add_space = previous.isTextNode() && value.empty();
      inferred_label =
          CombineAndCollapseWhitespace(value, inferred_label, add_space);
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    string16 trimmed_label;
    TrimWhitespace(inferred_label, TRIM_ALL, &trimmed_label);
    if (!trimmed_label.empty())
      break;

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (HasTagName(previous, "img") || HasTagName(previous, "br"))
      continue;

    // We only expect <p> and <label> tags to contain the full label text.
    if (HasTagName(previous, "p") || HasTagName(previous, "label"))
      inferred_label = FindChildText(previous);

    break;
  }

  TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing list item,
// e.g. <li>Some Text<input ...><input ...><input ...></tr>
string16 InferLabelFromListItem(const WebFormControlElement& element) {
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName("li")) {
    parent = parent.parentNode();
  }

  if (!parent.isNull() && HasTagName(parent, "li"))
    return FindChildText(parent);

  return string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
string16 InferLabelFromTableColumn(const WebFormControlElement& element) {
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName("td")) {
    parent = parent.parentNode();
  }

  if (parent.isNull())
    return string16();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  string16 inferred_label;
  WebNode previous = parent.previousSibling();
  while (inferred_label.empty() && !previous.isNull()) {
    if (HasTagName(previous, "td") || HasTagName(previous, "th"))
      inferred_label = FindChildText(previous);

    previous = previous.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
string16 InferLabelFromTableRow(const WebFormControlElement& element) {
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName("tr")) {
    parent = parent.parentNode();
  }

  if (parent.isNull())
    return string16();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  string16 inferred_label;
  WebNode previous = parent.previousSibling();
  while (inferred_label.empty() && !previous.isNull()) {
    if (HasTagName(previous, "tr"))
      inferred_label = FindChildText(previous);

    previous = previous.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding div table,
// e.g. <div>Some Text<span><input ...></span></div>
// e.g. <div>Some Text</div><div><input ...></div>
string16 InferLabelFromDivTable(const WebFormControlElement& element) {
  WebNode node = element.parentNode();
  bool looking_for_parent = true;

  // Search the sibling and parent <div>s until we find a candidate label.
  string16 inferred_label;
  while (inferred_label.empty() && !node.isNull()) {
    if (HasTagName(node, "div")) {
      looking_for_parent = false;
      inferred_label = FindChildText(node);
    } else if (looking_for_parent &&
               (HasTagName(node, "table") || HasTagName(node, "fieldset"))) {
      // If the element is in a table or fieldset, its label most likely is too.
      break;
    }

    if (node.previousSibling().isNull()) {
      // If there are no more siblings, continue walking up the tree.
      looking_for_parent = true;
    }

    if (looking_for_parent)
      node = node.parentNode();
    else
      node = node.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
string16 InferLabelFromDefinitionList(const WebFormControlElement& element) {
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName("dd"))
    parent = parent.parentNode();

  if (parent.isNull() || !HasTagName(parent, "dd"))
    return string16();

  // Skip by any intervening text nodes.
  WebNode previous = parent.previousSibling();
  while (!previous.isNull() && previous.isTextNode())
    previous = previous.previousSibling();

  if (previous.isNull() || !HasTagName(previous, "dt"))
    return string16();

  return FindChildText(previous);
}

// Infers corresponding label for |element| from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element.
string16 InferLabelForElement(const WebFormControlElement& element) {
  string16 inferred_label = InferLabelFromPrevious(element);
  if (!inferred_label.empty())
    return inferred_label;

  // If we didn't find a label, check for list item case.
  inferred_label = InferLabelFromListItem(element);
  if (!inferred_label.empty())
    return inferred_label;

  // If we didn't find a label, check for table cell case.
  inferred_label = InferLabelFromTableColumn(element);
  if (!inferred_label.empty())
    return inferred_label;

  // If we didn't find a label, check for table row case.
  inferred_label = InferLabelFromTableRow(element);
  if (!inferred_label.empty())
    return inferred_label;

  // If we didn't find a label, check for definition list case.
  inferred_label = InferLabelFromDefinitionList(element);
  if (!inferred_label.empty())
    return inferred_label;

  // If we didn't find a label, check for div table case.
  return InferLabelFromDivTable(element);
}

// Fills |option_strings| with the values of the <option> elements present in
// |select_element|.
void GetOptionStringsFromElement(const WebSelectElement& select_element,
                                 std::vector<string16>* option_values,
                                 std::vector<string16>* option_contents) {
  DCHECK(!select_element.isNull());

  option_values->clear();
  option_contents->clear();
  WebVector<WebElement> list_items = select_element.listItems();
  option_values->reserve(list_items.size());
  option_contents->reserve(list_items.size());
  for (size_t i = 0; i < list_items.size(); ++i) {
    if (IsOptionElement(list_items[i])) {
      const WebOptionElement option = list_items[i].toConst<WebOptionElement>();
      option_values->push_back(option.value());
      option_contents->push_back(option.text());
    }
  }
}

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
const string16 GetFormIdentifier(const WebFormElement& form) {
  string16 identifier = form.name();
  if (identifier.empty())
    identifier = form.getAttribute(WebString("id"));

  return identifier;
}

// The callback type used by |ForEachMatchingFormField()|.
typedef void (*Callback)(WebKit::WebFormControlElement*,
                         const webkit_glue::FormField*,
                         bool);

// For each autofillable field in |data| that matches a field in the |form|,
// the |callback| is invoked with the corresponding |form| field data.
void ForEachMatchingFormField(
    std::vector<WebKit::WebFormControlElement>* control_elements,
    const WebNode& node,
    const FormData& data,
    Callback callback) {
  // It's possible that the site has injected fields into the form after the
  // page has loaded, so we can't assert that the size of the cached control
  // elements is equal to the size of the fields in |form|.  Fortunately, the
  // one case in the wild where this happens, paypal.com signup form, the fields
  // are appended to the end of the form and are not visible.
  for (size_t i = 0, j = 0;
       i < control_elements->size() && j < data.fields.size();
       ++i) {
    WebFormControlElement* element = &(*control_elements)[i];
    string16 element_name(element->nameForAutofill());

    // Search forward in the |form| for a corresponding field.
    size_t k = j;
    while (k < data.fields.size() && element_name != data.fields[k].name)
      k++;

    if (k >= data.fields.size())
      continue;

    DCHECK_EQ(data.fields[k].name, element_name);

    bool is_initiating_node = false;

    const WebInputElement* input_element = toWebInputElement(element);
    if (IsTextInput(input_element)) {
      // TODO(jhawkins): WebKit currently doesn't handle the autocomplete
      // attribute for select control elements, but it probably should.
      if (!input_element->autoComplete())
        continue;

      is_initiating_node = (*input_element == node);

      // Only autofill empty fields and the field that initiated the filling,
      // i.e. the field the user is currently editing and interacting with.
      if (!is_initiating_node && !input_element->value().isEmpty())
        continue;
    }

    if (!element->isEnabled() || element->isReadOnly() ||
        !element->isFocusable())
      continue;

    callback(element, &data.fields[k], is_initiating_node);

    // We found a matching form field so move on to the next.
    ++j;
  }
}

// Sets the |field|'s value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void FillFormField(WebKit::WebFormControlElement* field,
                   const webkit_glue::FormField* data,
                   bool is_initiating_node) {
  // Nothing to fill.
  if (data->value.empty())
    return;

  WebInputElement* input_element = toWebInputElement(field);
  if (IsTextInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element->setValue(
        data->value.substr(0, input_element->maxLength()), true);
    input_element->setAutofilled(true);
    if (is_initiating_node) {
      int length = input_element->value().length();
      input_element->setSelectionRange(length, length);
    }
  } else {
    DCHECK(IsSelectElement(*field));
    WebSelectElement select_element = field->to<WebSelectElement>();
    if (select_element.value() != data->value) {
      select_element.setValue(data->value);
      select_element.dispatchFormControlChangeEvent();
    }
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void PreviewFormField(WebKit::WebFormControlElement* field,
                      const webkit_glue::FormField* data,
                      bool is_initiating_node) {
  // Nothing to preview.
  if (data->value.empty())
    return;

  // Only preview input fields.
  WebInputElement* input_element = toWebInputElement(field);
  if (!IsTextInput(input_element))
    return;

  // If the maxlength attribute contains a negative value, maxLength()
  // returns the default maxlength value.
  input_element->setSuggestedValue(
      data->value.substr(0, input_element->maxLength()));
  input_element->setAutofilled(true);
  if (is_initiating_node) {
    // Select the part of the text that the user didn't type.
    input_element->setSelectionRange(input_element->value().length(),
                                     input_element->suggestedValue().length());
  }
}

}  // namespace

namespace autofill {

struct FormManager::FormElement {
  WebKit::WebFormElement form_element;
  std::vector<WebKit::WebFormControlElement> control_elements;
  std::vector<string16> control_values;
};

FormManager::FormManager() {
}

FormManager::~FormManager() {
}

// static
void FormManager::WebFormControlElementToFormField(
    const WebFormControlElement& element,
    ExtractMask extract_mask,
    FormField* field) {
  DCHECK(field);
  DCHECK(!element.isNull());

  // The label is not officially part of a WebFormControlElement; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // WebFormElementToFormData.
  field->name = element.nameForAutofill();
  field->form_control_type = element.formControlType();

  if (!IsAutofillableElement(element))
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (IsTextInput(input_element)) {
    field->max_length = input_element->maxLength();
    field->is_autofilled = input_element->isAutofilled();
  } else if (extract_mask & EXTRACT_OPTIONS) {
    // Set option strings on the field if available.
    DCHECK(IsSelectElement(element));
    const WebSelectElement select_element = element.toConst<WebSelectElement>();
    GetOptionStringsFromElement(select_element,
                                &field->option_values,
                                &field->option_contents);
  }

  if (!(extract_mask & EXTRACT_VALUE))
    return;

  string16 value;
  if (IsTextInput(input_element)) {
    value = input_element->value();
  } else {
    DCHECK(IsSelectElement(element));
    const WebSelectElement select_element = element.toConst<WebSelectElement>();
    value = select_element.value();

    // Convert the |select_element| value to text if requested.
    if (extract_mask & EXTRACT_OPTION_TEXT) {
      WebVector<WebElement> list_items = select_element.listItems();
      for (size_t i = 0; i < list_items.size(); ++i) {
        if (IsOptionElement(list_items[i])) {
          const WebOptionElement option_element =
              list_items[i].toConst<WebOptionElement>();
          if (option_element.value() == value) {
            value = option_element.text();
            break;
          }
        }
      }
    }
  }

  // Constrain the maximum data length to prevent a malicious site from DOS'ing
  // the browser: http://crbug.com/49332
  if (value.size() > kMaxDataLength)
    value = value.substr(0, kMaxDataLength);

  field->value = value;
}

// static
string16 FormManager::LabelForElement(const WebFormControlElement& element) {
  // Don't scrape labels for elements we can't possibly autofill anyway.
  if (!IsAutofillableElement(element))
    return string16();

  WebNodeList labels = element.document().getElementsByTagName("label");
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebLabelElement label = labels.item(i).to<WebLabelElement>();
    WebElement corresponding_control = label.correspondingControl();
    if (corresponding_control == element ||
        (corresponding_control.isNull() &&
         label.getAttribute("for") == element.nameForAutofill())) {
      return FindChildText(label);
    }
  }

  // Infer the label from context if not found in label element.
  return InferLabelForElement(element);
}

// static
bool FormManager::WebFormElementToFormData(const WebFormElement& element,
                                           RequirementsMask requirements,
                                           ExtractMask extract_mask,
                                           FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.document().frame();
  if (!frame)
    return false;

  if (requirements & REQUIRE_AUTOCOMPLETE && !element.autoComplete())
    return false;

  form->name = GetFormIdentifier(element);
  form->method = element.method();
  form->origin = frame->document().url();
  form->action = frame->document().completeURL(element.action());
  form->user_submitted = element.wasUserSubmitted();

  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(element.action());

  // A map from a FormField's name to the FormField itself.
  std::map<string16, FormField*> name_map;

  // The extracted FormFields.  We use pointers so we can store them in
  // |name_map|.
  ScopedVector<FormField> form_fields;

  WebVector<WebFormControlElement> control_elements;
  element.getFormControlElements(control_elements);

  // A vector of bools that indicate whether each field in the form meets the
  // requirements and thus will be in the resulting |form|.
  std::vector<bool> fields_extracted(control_elements.size(), false);

  for (size_t i = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (!IsAutofillableElement(control_element))
      continue;

    const WebInputElement* input_element = toWebInputElement(&control_element);
    if (requirements & REQUIRE_AUTOCOMPLETE && IsTextInput(input_element) &&
        !input_element->autoComplete())
      continue;

    if (requirements & REQUIRE_ENABLED && !control_element.isEnabled())
      continue;

    // Create a new FormField, fill it out and map it to the field's name.
    FormField* field = new FormField;
    WebFormControlElementToFormField(control_element, extract_mask, field);
    form_fields.push_back(field);
    // TODO(jhawkins): A label element is mapped to a form control element's id.
    // field->name() will contain the id only if the name does not exist.  Add
    // an id() method to WebFormControlElement and use that here.
    name_map[field->name] = field;
    fields_extracted[i] = true;
  }

  // If we failed to extract any fields, give up.
  if (form_fields.empty())
    return false;

  // Loop through the label elements inside the form element.  For each label
  // element, get the corresponding form control element, use the form control
  // element's name as a key into the <name, FormField> map to find the
  // previously created FormField and set the FormField's label to the
  // label.firstChild().nodeValue() of the label element.
  WebNodeList labels = element.getElementsByTagName("label");
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebLabelElement label = labels.item(i).to<WebLabelElement>();
    WebFormControlElement field_element =
        label.correspondingControl().to<WebFormControlElement>();

    string16 element_name;
    if (field_element.isNull()) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      element_name = label.getAttribute("for");
    } else if (
        !field_element.isFormControlElement() ||
        field_element.formControlType() == WebString::fromUTF8("hidden")) {
      continue;
    } else {
      element_name = field_element.nameForAutofill();
    }

    std::map<string16, FormField*>::iterator iter = name_map.find(element_name);
    if (iter != name_map.end()) {
      string16 label_text = FindChildText(label);

      // Concatenate labels because some sites might have multiple label
      // candidates.
      if (!iter->second->label.empty() && !label_text.empty())
        iter->second->label += ASCIIToUTF16(" ");
      iter->second->label += label_text;
    }
  }

  // Loop through the form control elements, extracting the label text from
  // the DOM.  We use the |fields_extracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  for (size_t i = 0, field_idx = 0;
       i < control_elements.size() && field_idx < form_fields.size(); ++i) {
    // This field didn't meet the requirements, so don't try to find a label
    // for it.
    if (!fields_extracted[i])
      continue;

    const WebFormControlElement& control_element = control_elements[i];
    if (form_fields[field_idx]->label.empty())
      form_fields[field_idx]->label = InferLabelForElement(control_element);

    ++field_idx;
  }

  // Copy the created FormFields into the resulting FormData object.
  for (ScopedVector<FormField>::const_iterator iter = form_fields.begin();
       iter != form_fields.end(); ++iter) {
    form->fields.push_back(**iter);
  }

  return true;
}

void FormManager::ExtractForms(const WebFrame* frame,
                               std::vector<FormData>* forms) {
  // Reset the vector of FormElements for this frame.
  ResetFrame(frame);

  WebVector<WebFormElement> web_forms;
  frame->document().forms(web_forms);

  size_t num_fields_seen = 0;
  for (size_t i = 0; i < web_forms.size(); ++i) {
    // Owned by |form_elements_|.
    FormElement* form_element = new FormElement;
    form_element->form_element = web_forms[i];

    WebVector<WebFormControlElement> control_elements;
    form_element->form_element.getFormControlElements(control_elements);
    for (size_t j = 0; j < control_elements.size(); ++j) {
      WebFormControlElement element = control_elements[j];
      if (!IsAutofillableElement(element))
        continue;

      form_element->control_elements.push_back(element);

      // Save original values of <select> elements so we can restore them
      // when |ClearFormWithNode()| is invoked.
      if (IsSelectElement(element)) {
        form_element->control_values.push_back(
            element.toConst<WebSelectElement>().value());
      } else {
        form_element->control_values.push_back(string16());
      }
    }

    form_elements_.push_back(form_element);

    // To avoid overly expensive computation, we impose both a minimum and a
    // maximum number of allowable fields.
    if (form_element->control_elements.size() < kRequiredAutofillFields ||
        form_element->control_elements.size() > kMaxParseableFields)
      continue;

    FormData form;
    WebFormElementToFormData(
        form_element->form_element, REQUIRE_NONE, EXTRACT_VALUE, &form);

    num_fields_seen += form.fields.size();
    if (num_fields_seen > kMaxParseableFields)
      break;

    if (form.fields.size() >= kRequiredAutofillFields)
      forms->push_back(form);
  }
}

bool FormManager::FindFormWithFormControlElement(
    const WebFormControlElement& element,
    FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.document().frame();
  if (!frame)
    return false;

  for (FormElementList::const_iterator iter = form_elements_.begin();
       iter != form_elements_.end(); ++iter) {
    const FormElement* form_element = *iter;

    if (form_element->form_element.document().frame() != frame)
      continue;

    for (std::vector<WebFormControlElement>::const_iterator iter =
             form_element->control_elements.begin();
         iter != form_element->control_elements.end(); ++iter) {
      if (iter->nameForAutofill() == element.nameForAutofill()) {
        ExtractMask extract_mask =
            static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS);
        return WebFormElementToFormData(form_element->form_element,
                                        REQUIRE_NONE,
                                        extract_mask,
                                        form);
      }
    }
  }

  return false;
}

void FormManager::FillForm(const FormData& form, const WebNode& node) {
  FormElement* form_element = NULL;
  if (!FindCachedFormElement(form, &form_element))
    return;

  ForEachMatchingFormField(&form_element->control_elements,
                           node,
                           form,
                           &FillFormField);
}

void FormManager::PreviewForm(const FormData& form, const WebNode& node) {
  FormElement* form_element = NULL;
  if (!FindCachedFormElement(form, &form_element))
    return;

  ForEachMatchingFormField(&form_element->control_elements,
                           node,
                           form,
                           &PreviewFormField);
}

bool FormManager::ShowPredictions(const FormDataPredictions& form) {
  FormElement* form_element = NULL;
  if (!FindCachedFormElement(form.data, &form_element))
    return false;

  DCHECK_EQ(form.data.fields.size(), form.fields.size());
  for (size_t i = 0, j = 0;
       i < form_element->control_elements.size() && j < form.fields.size();
       ++i) {
    WebFormControlElement* element = &form_element->control_elements[i];
    string16 element_name(element->nameForAutofill());

    // Search forward in the |form| for a corresponding field.
    size_t k = j;
    while (k < form.fields.size() && element_name != form.data.fields[k].name)
      k++;

    if (k >= form.fields.size())
      continue;

    DCHECK_EQ(form.data.fields[k].name, element_name);

    std::string placeholder = form.fields[k].overall_type;
    string16 title = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SHOW_PREDICTIONS_TITLE,
        UTF8ToUTF16(form.fields[k].heuristic_type),
        UTF8ToUTF16(form.fields[k].server_type),
        UTF8ToUTF16(form.fields[k].signature),
        UTF8ToUTF16(form.signature),
        UTF8ToUTF16(form.experiment_id));
    if (!element->hasAttribute("placeholder"))
      element->setAttribute("placeholder", WebString(UTF8ToUTF16(placeholder)));
    element->setAttribute("title", WebString(title));

    // We found a matching form field so move on to the next.
    ++j;
  }

  return true;
}

bool FormManager::ClearFormWithNode(const WebNode& node) {
  FormElement* form_element = NULL;
  if (!FindCachedFormElementWithNode(node, &form_element))
    return false;

  for (size_t i = 0; i < form_element->control_elements.size(); ++i) {
    WebFormControlElement element = form_element->control_elements[i];
    WebInputElement* input_element = toWebInputElement(&element);
    if (IsTextInput(input_element)) {

      // We don't modify the value of disabled fields.
      if (!input_element->isEnabled())
        continue;

      input_element->setValue(string16(), true);
      input_element->setAutofilled(false);

      // Clearing the value in the focused node (above) can cause selection
      // to be lost. We force selection range to restore the text cursor.
      if (node == *input_element) {
        int length = input_element->value().length();
        input_element->setSelectionRange(length, length);
      }
    } else {
      DCHECK(IsSelectElement(element));
      WebSelectElement select_element = element.to<WebSelectElement>();
      if (select_element.value() != form_element->control_values[i]) {
        select_element.setValue(form_element->control_values[i]);
        select_element.dispatchFormControlChangeEvent();
      }
    }
  }

  return true;
}

bool FormManager::ClearPreviewedFormWithNode(const WebNode& node,
                                             bool was_autofilled) {
  FormElement* form_element = NULL;
  if (!FindCachedFormElementWithNode(node, &form_element))
    return false;

  for (size_t i = 0; i < form_element->control_elements.size(); ++i) {
    WebInputElement* input_element =
        toWebInputElement(&form_element->control_elements[i]);
    // Only text input elements can be previewed.
    if (!IsTextInput(input_element))
      continue;

    // If the input element has not been auto-filled, FormManager has not
    // previewed this field, so we have nothing to reset.
    if (!input_element->isAutofilled())
      continue;

    // There might be unrelated elements in this form which have already been
    // auto-filled. For example, the user might have already filled the address
    // part of a form and now be dealing with the credit card section. We only
    // want to reset the auto-filled status for fields that were previewed.
    if (input_element->suggestedValue().isEmpty())
      continue;

    // Clear the suggested value. For the initiating node, also restore the
    // original value.
    input_element->setSuggestedValue(WebString());
    bool is_initiating_node = (node == *input_element);
    if (is_initiating_node)
      input_element->setAutofilled(was_autofilled);
    else
      input_element->setAutofilled(false);

    // Clearing the suggested value in the focused node (above) can cause
    // selection to be lost. We force selection range to restore the text
    // cursor.
    if (is_initiating_node) {
      int length = input_element->value().length();
      input_element->setSelectionRange(length, length);
    }
  }

  return true;
}

void FormManager::ResetFrame(const WebFrame* frame) {
  FormElementList::iterator iter = form_elements_.begin();
  while (iter != form_elements_.end()) {
    if ((*iter)->form_element.document().frame() == frame)
      iter = form_elements_.erase(iter);
    else
      ++iter;
  }
}

bool FormManager::FormWithNodeIsAutofilled(const WebNode& node) {
  FormElement* form_element = NULL;
  if (!FindCachedFormElementWithNode(node, &form_element))
    return false;

  for (size_t i = 0; i < form_element->control_elements.size(); ++i) {
    WebInputElement* input_element =
        toWebInputElement(&form_element->control_elements[i]);
    if (!IsTextInput(input_element))
      continue;

    if (input_element->isAutofilled())
      return true;
  }

  return false;
}

bool FormManager::FindCachedFormElementWithNode(const WebNode& node,
                                                FormElement** form_element) {
  for (FormElementList::const_iterator form_iter = form_elements_.begin();
       form_iter != form_elements_.end(); ++form_iter) {
    for (std::vector<WebFormControlElement>::const_iterator iter =
             (*form_iter)->control_elements.begin();
         iter != (*form_iter)->control_elements.end(); ++iter) {
      if (*iter == node) {
        *form_element = *form_iter;
        return true;
      }
    }
  }

  return false;
}

bool FormManager::FindCachedFormElement(const FormData& form,
                                        FormElement** form_element) {
  for (FormElementList::iterator form_iter = form_elements_.begin();
       form_iter != form_elements_.end(); ++form_iter) {
    // TODO(dhollowa): matching on form name here which is not guaranteed to
    // be unique for the page, nor is it guaranteed to be non-empty.  Need to
    // find a way to uniquely identify the form cross-process.  For now we'll
    // check form name and form action for identity.
    // http://crbug.com/37990 test file sample8.html.
    // Also note that WebString() == WebString(string16()) does not evaluate to
    // |true| -- WebKit distinguishes between a "null" string (lhs) and an
    // "empty" string (rhs).  We don't want that distinction, so forcing to
    // string16.
    string16 element_name = GetFormIdentifier((*form_iter)->form_element);
    GURL action(
        (*form_iter)->form_element.document().completeURL(
            (*form_iter)->form_element.action()));
    if (element_name == form.name && action == form.action) {
      *form_element = *form_iter;
      return true;
    }
  }

  return false;
}

}  // namespace autofill
