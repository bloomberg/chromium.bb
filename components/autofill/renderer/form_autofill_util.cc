// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/renderer/form_autofill_util.h"

#include <map>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/common/autofill_switches.h"
#include "components/autofill/common/form_data.h"
#include "components/autofill/common/form_field_data.h"
#include "components/autofill/common/web_element_descriptor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebLabelElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebOptionElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSelectElement.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebExceptionCode;
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

namespace {

using autofill::ExtractAutofillableElements;
using autofill::IsAutofillableInputElement;
using autofill::IsCheckableElement;
using autofill::IsSelectElement;
using autofill::IsTextInput;

// The maximum length allowed for form data.
const size_t kMaxDataLength = 1024;

bool IsOptionElement(const WebElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kOption, ("option"));
  return element.hasTagName(kOption);
}

bool IsScriptElement(const WebElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kScript, ("script"));
  return element.hasTagName(kScript);
}

bool IsNoScriptElement(const WebElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kNoScript, ("noscript"));
  return element.hasTagName(kNoScript);
}

bool HasTagName(const WebNode& node, const WebKit::WebString& tag) {
  return node.isElementNode() && node.toConst<WebElement>().hasTagName(tag);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement* input_element = toWebInputElement(&element);
  return IsAutofillableInputElement(input_element) || IsSelectElement(element);
}

// Check whether the given field satisfies the REQUIRE_AUTOCOMPLETE requirement.
// When Autocheckout is enabled, this requirement is enforced in the browser
// process rather than in the renderer process, and hence all fields are
// considered to satisfy this requirement.
bool SatisfiesRequireAutocomplete(const WebInputElement& input_element) {
  return input_element.autoComplete() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalFormFilling);
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
    CR_DEFINE_STATIC_LOCAL(WebString, kBold, ("b"));
    CR_DEFINE_STATIC_LOCAL(WebString, kStrong, ("strong"));
    CR_DEFINE_STATIC_LOCAL(WebString, kSpan, ("span"));
    CR_DEFINE_STATIC_LOCAL(WebString, kFont, ("font"));
    if (previous.isTextNode() ||
        HasTagName(previous, kBold) || HasTagName(previous, kStrong) ||
        HasTagName(previous, kSpan) || HasTagName(previous, kFont)) {
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
    CR_DEFINE_STATIC_LOCAL(WebString, kImage, ("img"));
    CR_DEFINE_STATIC_LOCAL(WebString, kBreak, ("br"));
    if (HasTagName(previous, kImage) || HasTagName(previous, kBreak))
      continue;

    // We only expect <p> and <label> tags to contain the full label text.
    CR_DEFINE_STATIC_LOCAL(WebString, kPage, ("p"));
    CR_DEFINE_STATIC_LOCAL(WebString, kLabel, ("label"));
    if (HasTagName(previous, kPage) || HasTagName(previous, kLabel))
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
  CR_DEFINE_STATIC_LOCAL(WebString, kListItem, ("li"));
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName(kListItem)) {
    parent = parent.parentNode();
  }

  if (!parent.isNull() && HasTagName(parent, kListItem))
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
  CR_DEFINE_STATIC_LOCAL(WebString, kTableCell, ("td"));
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName(kTableCell)) {
    parent = parent.parentNode();
  }

  if (parent.isNull())
    return string16();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  string16 inferred_label;
  WebNode previous = parent.previousSibling();
  CR_DEFINE_STATIC_LOCAL(WebString, kTableHeader, ("th"));
  while (inferred_label.empty() && !previous.isNull()) {
    if (HasTagName(previous, kTableCell) || HasTagName(previous, kTableHeader))
      inferred_label = FindChildText(previous);

    previous = previous.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
string16 InferLabelFromTableRow(const WebFormControlElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kTableRow, ("tr"));
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName(kTableRow)) {
    parent = parent.parentNode();
  }

  if (parent.isNull())
    return string16();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  string16 inferred_label;
  WebNode previous = parent.previousSibling();
  while (inferred_label.empty() && !previous.isNull()) {
    if (HasTagName(previous, kTableRow))
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
  CR_DEFINE_STATIC_LOCAL(WebString, kDiv, ("div"));
  CR_DEFINE_STATIC_LOCAL(WebString, kTable, ("table"));
  CR_DEFINE_STATIC_LOCAL(WebString, kFieldSet, ("fieldset"));
  while (inferred_label.empty() && !node.isNull()) {
    if (HasTagName(node, kDiv)) {
      looking_for_parent = false;
      inferred_label = FindChildText(node);
    } else if (looking_for_parent &&
               (HasTagName(node, kTable) || HasTagName(node, kFieldSet))) {
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
  CR_DEFINE_STATIC_LOCAL(WebString, kDefinitionData, ("dd"));
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasTagName(kDefinitionData))
    parent = parent.parentNode();

  if (parent.isNull() || !HasTagName(parent, kDefinitionData))
    return string16();

  // Skip by any intervening text nodes.
  WebNode previous = parent.previousSibling();
  while (!previous.isNull() && previous.isTextNode())
    previous = previous.previousSibling();

  CR_DEFINE_STATIC_LOCAL(WebString, kDefinitionTag, ("dt"));
  if (previous.isNull() || !HasTagName(previous, kDefinitionTag))
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

// The callback type used by |ForEachMatchingFormField()|.
typedef void (*Callback)(const FormFieldData&,
                         bool, /* is_initiating_element */
                         WebKit::WebFormControlElement*);

// For each autofillable field in |data| that matches a field in the |form|,
// the |callback| is invoked with the corresponding |form| field data.
void ForEachMatchingFormField(const WebFormElement& form_element,
                              const WebElement& initiating_element,
                              const FormData& data,
                              bool only_focusable_elements,
                              bool force_override,
                              Callback callback) {
  std::vector<WebFormControlElement> control_elements;
  ExtractAutofillableElements(form_element, autofill::REQUIRE_AUTOCOMPLETE,
                              &control_elements);

  if (control_elements.size() != data.fields.size()) {
    // This case should be reachable only for pathological websites and tests,
    // which add or remove form fields while the user is interacting with the
    // Autofill popup.
    return;
  }

  // It's possible that the site has injected fields into the form after the
  // page has loaded, so we can't assert that the size of the cached control
  // elements is equal to the size of the fields in |form|.  Fortunately, the
  // one case in the wild where this happens, paypal.com signup form, the fields
  // are appended to the end of the form and are not visible.
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement* element = &control_elements[i];

    if (string16(element->nameForAutofill()) != data.fields[i].name) {
      // This case should be reachable only for pathological websites, which
      // rename form fields while the user is interacting with the Autofill
      // popup.  I (isherman) am not aware of any such websites, and so am
      // optimistically including a NOTREACHED().  If you ever trip this check,
      // please file a bug against me.
      NOTREACHED();
      continue;
    }

    bool is_initiating_element = (*element == initiating_element);

    // Only autofill empty fields and the field that initiated the filling,
    // i.e. the field the user is currently editing and interacting with.
    const WebInputElement* input_element = toWebInputElement(element);
    if (!force_override && IsTextInput(input_element) &&
        !is_initiating_element && !input_element->value().isEmpty())
      continue;

    if (!element->isEnabled() || element->isReadOnly() ||
        (only_focusable_elements && !element->isFocusable()))
      continue;

    callback(data.fields[i], is_initiating_element, element);
  }
}

// Sets the |field|'s value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void FillFormField(const FormFieldData& data,
                   bool is_initiating_node,
                   WebKit::WebFormControlElement* field) {
  // Nothing to fill.
  if (data.value.empty())
    return;

  WebInputElement* input_element = toWebInputElement(field);
  if (IsTextInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element->setValue(
        data.value.substr(0, input_element->maxLength()), true);
    input_element->setAutofilled(true);
    if (is_initiating_node) {
      int length = input_element->value().length();
      input_element->setSelectionRange(length, length);
      // Clear the current IME composition (the underline), if there is one.
      input_element->document().frame()->unmarkText();
    }
  } else if (IsSelectElement(*field)) {
    WebSelectElement select_element = field->to<WebSelectElement>();
    if (select_element.value() != data.value) {
      select_element.setValue(data.value);
      select_element.dispatchFormControlChangeEvent();
    }
  } else {
    DCHECK(IsCheckableElement(input_element));
    input_element->setChecked(data.is_checked, true);
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void PreviewFormField(const FormFieldData& data,
                      bool is_initiating_node,
                      WebKit::WebFormControlElement* field) {
  // Nothing to preview.
  if (data.value.empty())
    return;

  // Only preview input fields. Excludes checkboxes and radio buttons, as there
  // is no provision for setSuggestedCheckedValue in WebInputElement.
  WebInputElement* input_element = toWebInputElement(field);
  if (!IsTextInput(input_element))
    return;

  // If the maxlength attribute contains a negative value, maxLength()
  // returns the default maxlength value.
  input_element->setSuggestedValue(
      data.value.substr(0, input_element->maxLength()));
  input_element->setAutofilled(true);
  if (is_initiating_node) {
    // Select the part of the text that the user didn't type.
    input_element->setSelectionRange(input_element->value().length(),
                                     input_element->suggestedValue().length());
  }
}

std::string RetrievalMethodToString(
    const autofill::WebElementDescriptor::RetrievalMethod& method) {
  switch (method) {
    case autofill::WebElementDescriptor::CSS_SELECTOR:
      return "CSS_SELECTOR";
    case autofill::WebElementDescriptor::ID:
      return "ID";
    case autofill::WebElementDescriptor::NONE:
      return "NONE";
  }
  NOTREACHED();
  return "UNKNOWN";
}

}  // namespace

namespace autofill {

const size_t kMaxParseableFields = 100;

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement* element) {
  return element && element->isTextField();
}

bool IsSelectElement(const WebFormControlElement& element) {
  // Is static for improving performance.
  CR_DEFINE_STATIC_LOCAL(WebString, kSelectOne, ("select-one"));
  return element.formControlType() == kSelectOne;
}

bool IsCheckableElement(const WebInputElement* element) {
  if (!element)
    return false;

  return element->isCheckbox() || element->isRadioButton();
}

bool IsAutofillableInputElement(const WebInputElement* element) {
  return IsTextInput(element) || IsCheckableElement(element);
}

const string16 GetFormIdentifier(const WebFormElement& form) {
  string16 identifier = form.name();
  CR_DEFINE_STATIC_LOCAL(WebString, kId, ("id"));
  if (identifier.empty())
    identifier = form.getAttribute(kId);

  return identifier;
}

bool ClickElement(const WebDocument& document,
                  const WebElementDescriptor& element_descriptor) {
  WebString web_descriptor = WebString::fromUTF8(element_descriptor.descriptor);
  WebKit::WebElement element;

  switch (element_descriptor.retrieval_method) {
    case WebElementDescriptor::CSS_SELECTOR: {
      WebExceptionCode ec = 0;
      element = document.querySelector(web_descriptor, ec);
      if (ec)
        DVLOG(1) << "Query selector failed. Error code: " << ec << ".";
      break;
    }
    case WebElementDescriptor::ID:
      element = document.getElementById(web_descriptor);
      break;
    case WebElementDescriptor::NONE:
      return true;
  }

  if (element.isNull()) {
    DVLOG(1) << "Could not find "
             << element_descriptor.descriptor
             << " by "
             << RetrievalMethodToString(element_descriptor.retrieval_method)
             << ".";
    return false;
  }

  element.simulateClick();
  return true;
}

// Fills |autofillable_elements| with all the auto-fillable form control
// elements in |form_element|.
void ExtractAutofillableElements(
    const WebFormElement& form_element,
    RequirementsMask requirements,
    std::vector<WebFormControlElement>* autofillable_elements) {
  WebVector<WebFormControlElement> control_elements;
  form_element.getFormControlElements(control_elements);

  autofillable_elements->clear();
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement element = control_elements[i];
    if (!IsAutofillableElement(element))
      continue;

    if (requirements & REQUIRE_AUTOCOMPLETE) {
      // TODO(jhawkins): WebKit currently doesn't handle the autocomplete
      // attribute for select control elements, but it probably should.
      WebInputElement* input_element = toWebInputElement(&control_elements[i]);
      if (IsAutofillableInputElement(input_element) &&
          !SatisfiesRequireAutocomplete(*input_element))
        continue;
    }

    autofillable_elements->push_back(element);
  }
}

void WebFormControlElementToFormField(const WebFormControlElement& element,
                                      ExtractMask extract_mask,
                                      FormFieldData* field) {
  DCHECK(field);
  DCHECK(!element.isNull());
  CR_DEFINE_STATIC_LOCAL(WebString, kAutocomplete, ("autocomplete"));

  // The label is not officially part of a WebFormControlElement; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // WebFormElementToFormData.
  field->name = element.nameForAutofill();
  field->form_control_type = UTF16ToUTF8(element.formControlType());
  field->autocomplete_attribute =
      UTF16ToUTF8(element.getAttribute(kAutocomplete));
  if (field->autocomplete_attribute.size() > kMaxDataLength) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process.  However, send over a default string to indicate that the
    // attribute was present.
    field->autocomplete_attribute = "x-max-data-length-exceeded";
  }

  if (!IsAutofillableElement(element))
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (IsAutofillableInputElement(input_element)) {
    if (IsTextInput(input_element))
      field->max_length = input_element->maxLength();

    field->is_autofilled = input_element->isAutofilled();
    field->is_focusable = input_element->isFocusable();
    field->should_autocomplete = input_element->autoComplete();
    field->is_checkable = IsCheckableElement(input_element);
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
  if (IsAutofillableInputElement(input_element)) {
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

bool WebFormElementToFormData(
    const WebKit::WebFormElement& form_element,
    const WebKit::WebFormControlElement& form_control_element,
    RequirementsMask requirements,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  CR_DEFINE_STATIC_LOCAL(WebString, kLabel, ("label"));
  CR_DEFINE_STATIC_LOCAL(WebString, kFor, ("for"));
  CR_DEFINE_STATIC_LOCAL(WebString, kHidden, ("hidden"));

  const WebFrame* frame = form_element.document().frame();
  if (!frame)
    return false;

  if (requirements & REQUIRE_AUTOCOMPLETE && !form_element.autoComplete())
    return false;

  form->name = GetFormIdentifier(form_element);
  form->method = form_element.method();
  form->origin = frame->document().url();
  form->action = frame->document().completeURL(form_element.action());
  form->user_submitted = form_element.wasUserSubmitted();

  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(form_element.action());

  // A map from a FormFieldData's name to the FormFieldData itself.
  std::map<string16, FormFieldData*> name_map;

  // The extracted FormFields.  We use pointers so we can store them in
  // |name_map|.
  ScopedVector<FormFieldData> form_fields;

  WebVector<WebFormControlElement> control_elements;
  form_element.getFormControlElements(control_elements);

  // A vector of bools that indicate whether each field in the form meets the
  // requirements and thus will be in the resulting |form|.
  std::vector<bool> fields_extracted(control_elements.size(), false);

  for (size_t i = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (!IsAutofillableElement(control_element))
      continue;

    const WebInputElement* input_element = toWebInputElement(&control_element);
    if (requirements & REQUIRE_AUTOCOMPLETE &&
        IsAutofillableInputElement(input_element) &&
        !SatisfiesRequireAutocomplete(*input_element))
      continue;

    // Create a new FormFieldData, fill it out and map it to the field's name.
    FormFieldData* form_field = new FormFieldData;
    WebFormControlElementToFormField(control_element, extract_mask, form_field);
    form_fields.push_back(form_field);
    // TODO(jhawkins): A label element is mapped to a form control element's id.
    // field->name() will contain the id only if the name does not exist.  Add
    // an id() method to WebFormControlElement and use that here.
    name_map[form_field->name] = form_field;
    fields_extracted[i] = true;
  }

  // If we failed to extract any fields, give up.  Also, to avoid overly
  // expensive computation, we impose a maximum number of allowable fields.
  if (form_fields.empty() || form_fields.size() > kMaxParseableFields)
    return false;

  // Loop through the label elements inside the form element.  For each label
  // element, get the corresponding form control element, use the form control
  // element's name as a key into the <name, FormFieldData> map to find the
  // previously created FormFieldData and set the FormFieldData's label to the
  // label.firstChild().nodeValue() of the label element.
  WebNodeList labels = form_element.getElementsByTagName(kLabel);
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebLabelElement label = labels.item(i).to<WebLabelElement>();
    WebFormControlElement field_element =
        label.correspondingControl().to<WebFormControlElement>();

    string16 element_name;
    if (field_element.isNull()) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      element_name = label.getAttribute(kFor);
    } else if (
        !field_element.isFormControlElement() ||
        field_element.formControlType() == kHidden) {
      continue;
    } else {
      element_name = field_element.nameForAutofill();
    }

    std::map<string16, FormFieldData*>::iterator iter =
        name_map.find(element_name);
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

    if (field && form_control_element == control_element)
      *field = *form_fields[field_idx];

    ++field_idx;
  }

  // Copy the created FormFields into the resulting FormData object.
  for (ScopedVector<FormFieldData>::const_iterator iter = form_fields.begin();
       iter != form_fields.end(); ++iter) {
    form->fields.push_back(**iter);
  }

  return true;
}

bool FindFormAndFieldForInputElement(const WebInputElement& element,
                                     FormData* form,
                                     FormFieldData* field,
                                     RequirementsMask requirements) {
  if (!IsAutofillableElement(element))
    return false;

  const WebFormElement form_element = element.form();
  if (form_element.isNull())
    return false;

  ExtractMask extract_mask =
      static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS);
  return WebFormElementToFormData(form_element,
                                  element,
                                  requirements,
                                  extract_mask,
                                  form,
                                  field);
}

void FillForm(const FormData& form, const WebInputElement& element) {
  WebFormElement form_element = element.form();
  if (form_element.isNull())
    return;

  ForEachMatchingFormField(form_element,
                           element,
                           form,
                           true, /* only_focusable_elements */
                           false, /* dont force override */
                           &FillFormField);
}

void FillFormIncludingNonFocusableElements(const FormData& form_data,
                                     const WebFormElement& form_element) {
  if (form_element.isNull())
    return;

  ForEachMatchingFormField(form_element,
                           WebInputElement(),
                           form_data,
                           false, /* only_focusable_elements */
                           true, /* force override */
                           &FillFormField);
}

void PreviewForm(const FormData& form, const WebInputElement& element) {
  WebFormElement form_element = element.form();
  if (form_element.isNull())
    return;

  ForEachMatchingFormField(form_element,
                           element,
                           form,
                           true, /* only_focusable_elements */
                           false, /* dont force override */
                           &PreviewFormField);
}

bool ClearPreviewedFormWithElement(const WebInputElement& element,
                                   bool was_autofilled) {
  WebFormElement form_element = element.form();
  if (form_element.isNull())
    return false;

  std::vector<WebFormControlElement> control_elements;
  ExtractAutofillableElements(form_element, REQUIRE_AUTOCOMPLETE,
                              &control_elements);
  for (size_t i = 0; i < control_elements.size(); ++i) {
    // Only text input elements can be previewed.
    WebInputElement* input_element = toWebInputElement(&control_elements[i]);
    if (!IsTextInput(input_element))
      continue;

    // If the input element is not auto-filled, we did not preview it, so there
    // is nothing to reset.
    if (!input_element->isAutofilled())
      continue;

    // There might be unrelated elements in this form which have already been
    // auto-filled.  For example, the user might have already filled the address
    // part of a form and now be dealing with the credit card section.  We only
    // want to reset the auto-filled status for fields that were previewed.
    if (input_element->suggestedValue().isEmpty())
      continue;

    // Clear the suggested value. For the initiating node, also restore the
    // original value.
    input_element->setSuggestedValue(WebString());
    bool is_initiating_node = (element == *input_element);
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

bool FormWithElementIsAutofilled(const WebInputElement& element) {
  WebFormElement form_element = element.form();
  if (form_element.isNull())
    return false;

  std::vector<WebFormControlElement> control_elements;
  ExtractAutofillableElements(form_element, REQUIRE_AUTOCOMPLETE,
                              &control_elements);
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebInputElement* input_element = toWebInputElement(&control_elements[i]);
    if (!IsAutofillableInputElement(input_element))
      continue;

    if (input_element->isAutofilled())
      return true;
  }

  return false;
}

}  // namespace autofill
