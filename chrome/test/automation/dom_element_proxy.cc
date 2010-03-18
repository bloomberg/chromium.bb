// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/dom_element_proxy.h"

#include "base/json/string_escape.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Convenience wrapper for GetDoubleQuotedJson function.
std::string GetDoubleQuotedJson(std::string utf8_string) {
  return base::GetDoubleQuotedJson(UTF8ToUTF16(utf8_string));
}

}  // namespace

DOMElementProxyRef DOMElementProxy::GetContentDocument() {
  const char* script = "%s.contentDocument;";
  DOMElementProxy* element = NULL;
  executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str()), &element);
  return element;
}

DOMElementProxyRef DOMElementProxy::GetDocumentFromFrame(
    const std::vector<std::string>& frame_names) {
  if (!is_valid())
    return NULL;

  const char* script = "domAutomation.getDocumentFromFrame(%s, %s);";
  std::string string_script = StringPrintf(
      script, this->GetReferenceJavaScript().c_str(),
      JavaScriptExecutionController::Serialize(frame_names).c_str());
  DOMElementProxy* element = NULL;
  executor_->ExecuteJavaScriptAndParse(string_script, &element);
  return element;
}

DOMElementProxyRef DOMElementProxy::GetDocumentFromFrame(
    const std::string& frame_name) {
  if (!is_valid())
    return NULL;

  std::vector<std::string> frame_names;
  frame_names.push_back(frame_name);
  return GetDocumentFromFrame(frame_names);
}

DOMElementProxyRef DOMElementProxy::GetDocumentFromFrame(
    const std::string& frame_name1, const std::string& frame_name2) {
  if (!is_valid())
    return NULL;

  std::vector<std::string> frame_names;
  frame_names.push_back(frame_name1);
  frame_names.push_back(frame_name2);
  return GetDocumentFromFrame(frame_names);
}

DOMElementProxyRef DOMElementProxy::GetDocumentFromFrame(
    const std::string& frame_name1, const std::string& frame_name2,
    const std::string& frame_name3) {
  if (!is_valid())
    return NULL;

  std::vector<std::string> frame_names;
  frame_names.push_back(frame_name1);
  frame_names.push_back(frame_name2);
  frame_names.push_back(frame_name3);
  return GetDocumentFromFrame(frame_names);
}

bool DOMElementProxy::FindByXPath(const std::string& xpath,
                                  std::vector<DOMElementProxyRef>* elements) {
  DCHECK(elements);
  if (!is_valid())
    return false;

  const char* script = "domAutomation.findByXPath(%s, %s);";
  std::vector<DOMElementProxy*> element_pointers;
  if (!executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(xpath).c_str()),
      &element_pointers))
    return false;
  for (size_t i = 0; i < element_pointers.size(); i++)
    elements->push_back(element_pointers[i]);
  return true;
}

DOMElementProxyRef DOMElementProxy::FindByXPath(const std::string& xpath) {
  if (!is_valid())
    return NULL;

  const char* script = "domAutomation.find1ByXPath(%s, %s);";
  DOMElementProxy* element = NULL;
  executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(xpath).c_str()),
      &element);
  return element;
}

bool DOMElementProxy::FindBySelectors(
    const std::string& selectors, std::vector<DOMElementProxyRef>* elements) {
  DCHECK(elements);
  if (!is_valid())
    return false;

  const char* script = "domAutomation.findBySelectors(%s, %s);";
  std::vector<DOMElementProxy*> element_pointers;
  if (!executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(selectors).c_str()),
      &element_pointers))
    return false;
  for (size_t i = 0; i < element_pointers.size(); i++)
    elements->push_back(element_pointers[i]);
  return true;
}

DOMElementProxyRef DOMElementProxy::FindBySelectors(
    const std::string& selectors) {
  if (!is_valid())
    return NULL;

  const char* script = "domAutomation.find1BySelectors(%s, %s);";
  DOMElementProxy* element = NULL;
  executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(selectors).c_str()),
      &element);
  return element;
}

bool DOMElementProxy::FindByText(const std::string& text,
                                 std::vector<DOMElementProxyRef>* elements) {
  DCHECK(elements);
  if (!is_valid())
    return false;

  const char* script = "domAutomation.findByText(%s, %s);";
  std::vector<DOMElementProxy*> element_pointers;
  if (!executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(text).c_str()),
      &element_pointers))
    return false;
  for (size_t i = 0; i < element_pointers.size(); i++)
    elements->push_back(element_pointers[i]);
  return true;
}

DOMElementProxyRef DOMElementProxy::FindByText(const std::string& text) {
  if (!is_valid())
    return NULL;

  const char* script = "domAutomation.find1ByText(%s, %s);";
  DOMElementProxy* element = NULL;
  executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(text).c_str()),
      &element);
  return element;
}

bool DOMElementProxy::Click() {
  const char* script = "domAutomation.click(%s);";
  if (!is_valid())
    return false;

  return executor_->ExecuteJavaScript(
      StringPrintf(script, this->GetReferenceJavaScript().c_str()));
}

bool DOMElementProxy::Type(const std::string& text) {
  const char* script = "domAutomation.type(%s, %s);";
  if (!is_valid())
    return false;

  bool success = false;
  executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(text).c_str()),
      &success);
  return success;
}

bool DOMElementProxy::SetText(const std::string& text) {
  const char* script = "domAutomation.setText(%s, %s);";
  if (!is_valid())
    return false;

  bool success = false;
  executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(text).c_str()),
      &success);
  return success;
}

bool DOMElementProxy::GetProperty(const std::string& property,
                                  std::string* out) {
  DCHECK(out);
  if (!is_valid())
    return false;

  const char* script = "%s.%s;";
  return executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(property).c_str()),
      out);
}

bool DOMElementProxy::GetAttribute(const std::string& attribute,
                                   std::string* out) {
  DCHECK(out);
  if (!is_valid())
    return false;

  const char* script = "%s.getAttribute(%s);";
  return executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str(),
                   GetDoubleQuotedJson(attribute).c_str()),
      out);
}

bool DOMElementProxy::GetText(std::string* text) {
  DCHECK(text);
  if (!is_valid())
    return false;

  const char* script = "domAutomation.getText(%s);";
  return executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str()), text);
}

bool DOMElementProxy::GetInnerHTML(std::string* html) {
  DCHECK(html);
  if (!is_valid())
    return false;

  const char* script = "domAutomation.getInnerHTML(%s);";
  return executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str()), html);
}

bool DOMElementProxy::GetId(std::string* id) {
  DCHECK(id);
  if (!is_valid())
    return false;

  const char* script = "%s.id;";
  return executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str()), id);
}

bool DOMElementProxy::GetName(std::string* name) {
  return GetAttribute("name", name);
}

bool DOMElementProxy::GetVisibility(bool* visibility) {
  DCHECK(visibility);
  if (!is_valid())
    return false;

  const char* script = "domAutomation.isVisible(%s);";
  return executor_->ExecuteJavaScriptAndParse(
      StringPrintf(script, this->GetReferenceJavaScript().c_str()),
      visibility);
}

void DOMElementProxy::EnsureTextMatches(const std::string& expected_text) {
  std::string text;
  ASSERT_TRUE(GetText(&text));
  ASSERT_EQ(expected_text, text);
}

void DOMElementProxy::EnsureInnerHTMLMatches(const std::string& expected_html) {
  std::string html;
  ASSERT_TRUE(GetInnerHTML(&html));
  ASSERT_EQ(expected_html, html);
}

void DOMElementProxy::EnsureNameMatches(const std::string& expected_name) {
  std::string name;
  ASSERT_TRUE(GetName(&name));
  ASSERT_EQ(expected_name, name);
}

void DOMElementProxy::EnsureVisibilityMatches(bool expected_visibility) {
  bool visibility;
  ASSERT_TRUE(GetVisibility(&visibility));
  ASSERT_EQ(expected_visibility, visibility);
}
