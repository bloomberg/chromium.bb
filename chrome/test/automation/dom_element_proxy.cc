// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/dom_element_proxy.h"

#include "chrome/test/automation/javascript_execution_controller.h"
#include "chrome/test/automation/javascript_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using javascript_utils::JavaScriptPrintf;

// JavaScriptObjectProxy methods
JavaScriptObjectProxy::JavaScriptObjectProxy(
    JavaScriptExecutionController* executor, int handle)
    : executor_(executor->AsWeakPtr()), handle_(handle) {}

JavaScriptObjectProxy::~JavaScriptObjectProxy() {
  if (is_valid())
    executor_->Remove(handle_);
}

// DOMElementProxy::By methods
// static
DOMElementProxy::By DOMElementProxy::By::XPath(
    const std::string& xpath) {
  return By(TYPE_XPATH, xpath);
}

// static
DOMElementProxy::By DOMElementProxy::By::Selectors(
    const std::string& selectors) {
  return By(TYPE_SELECTORS, selectors);
}

// static
DOMElementProxy::By DOMElementProxy::By::Text(
    const std::string& text) {
  return By(TYPE_TEXT, text);
}

// DOMElementProxy methods
DOMElementProxyRef DOMElementProxy::GetContentDocument() {
  int element_handle;
  if (!GetValue("contentdocument", &element_handle))
    return NULL;
  return executor_->GetObjectProxy<DOMElementProxy>(element_handle);
}

DOMElementProxyRef DOMElementProxy::GetDocumentFromFrame(
    const std::vector<std::string>& frame_names) {
  if (!is_valid())
    return NULL;

  const char* script = "domAutomation.getDocumentFromFrame("
                       "domAutomation.getObject(%s), %s);";
  int element_handle;
  if (!executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), frame_names),
      &element_handle)) {
    return NULL;
  }
  return executor_->GetObjectProxy<DOMElementProxy>(element_handle);
}

DOMElementProxyRef DOMElementProxy::GetDocumentFromFrame(
    const std::string& frame_name1, const std::string& frame_name2,
    const std::string& frame_name3) {
  if (!is_valid())
    return NULL;

  std::vector<std::string> frame_names;
  frame_names.push_back(frame_name1);
  if (!frame_name2.empty())
    frame_names.push_back(frame_name2);
  if (!frame_name3.empty())
    frame_names.push_back(frame_name3);
  return GetDocumentFromFrame(frame_names);
}

DOMElementProxyRef DOMElementProxy::FindElement(const By& by) {
  if (!is_valid())
    return NULL;

  const char* script = "domAutomation.findElement("
                       "domAutomation.getObject(%s), %s);";
  int element_handle;
  if (!executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), by), &element_handle)) {
    return NULL;
  }
  return executor_->GetObjectProxy<DOMElementProxy>(element_handle);
}

bool DOMElementProxy::FindElements(const By& by,
                                   std::vector<DOMElementProxyRef>* elements) {
  if (!elements) {
    NOTREACHED();
    return false;
  }
  if (!is_valid())
    return false;

  const char* script = "domAutomation.findElements("
                       "domAutomation.getObject(%s), %s);";
  std::vector<int> element_handles;
  if (!executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), by), &element_handles)) {
    return false;
  }
  for (size_t i = 0; i < element_handles.size(); i++) {
    elements->push_back(make_scoped_refptr(
          executor_->GetObjectProxy<DOMElementProxy>(element_handles[i])));
  }
  return true;
}

bool DOMElementProxy::WaitForVisibleElementCount(
    const By& by, int count, std::vector<DOMElementProxyRef>* elements) {
  if (!elements) {
    NOTREACHED();
    return false;
  }
  if (!is_valid())
    return false;

  const char* script = "domAutomation.waitForVisibleElementCount("
                       "domAutomation.getObject(%s), %s, %s,"
                       "domAutomation.getCallId());";
  std::vector<int> element_handles;
  if (!executor_->ExecuteAsyncJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), by, count), &element_handles)) {
    return false;
  }
  if (static_cast<int>(element_handles.size()) == count) {
    for (size_t i = 0; i < element_handles.size(); i++) {
      elements->push_back(make_scoped_refptr(
            executor_->GetObjectProxy<DOMElementProxy>(element_handles[i])));
    }
  }
  return static_cast<int>(element_handles.size()) == count;
}

DOMElementProxyRef DOMElementProxy::WaitFor1VisibleElement(const By& by) {
  std::vector<DOMElementProxyRef> elements;
  if (!WaitForVisibleElementCount(by, 1, &elements))
    return NULL;
  return elements[0];
}

bool DOMElementProxy::WaitForElementsToDisappear(const By& by) {
  std::vector<DOMElementProxyRef> elements;
  return WaitForVisibleElementCount(by, 0, &elements);
}

bool DOMElementProxy::Click() {
  if (!is_valid())
    return false;

  const char* script = "domAutomation.click(domAutomation.getObject(%s));";
  return executor_->ExecuteJavaScript(
      JavaScriptPrintf(script, this->handle()));
}

bool DOMElementProxy::Type(const std::string& text) {
  if (!is_valid())
    return false;

  const char* script = "domAutomation.type(domAutomation.getObject(%s), %s);";
  bool success = false;
  if (!executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), text), &success)) {
    return false;
  }
  return success;
}

bool DOMElementProxy::SetText(const std::string& text) {
  if (!is_valid())
    return false;

  const char* script = "domAutomation.setText("
                       "domAutomation.getObject(%s), %s);";
  bool success = false;
  if (!executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), text), &success)) {
    return false;
  }
  return success;
}

bool DOMElementProxy::GetProperty(const std::string& property,
                                  std::string* out) {
  if (!out) {
    NOTREACHED();
    return false;
  }
  if (!is_valid())
    return false;

  const char* script = "domAutomation.getProperty("
                       "domAutomation.getObject(%s), %s);";
  return executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), property), out);
}

bool DOMElementProxy::GetAttribute(const std::string& attribute,
                                   std::string* out) {
  if (!out) {
    NOTREACHED();
    return false;
  }
  if (!is_valid())
    return false;

  const char* script = "domAutomation.getAttribute("
                       "domAutomation.getObject(%s), %s);";
  return executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), attribute), out);
}

bool DOMElementProxy::GetText(std::string* text) {
  return GetValue("text", text);
}

bool DOMElementProxy::GetInnerHTML(std::string* html) {
  return GetValue("innerhtml", html);
}

bool DOMElementProxy::GetId(std::string* id) {
  return GetValue("id", id);
}

bool DOMElementProxy::GetName(std::string* name) {
  return GetAttribute("name", name);
}

bool DOMElementProxy::GetVisibility(bool* visibility) {
  return GetValue("visibility", visibility);
}

void DOMElementProxy::EnsureFindNoElements(const By& by) {
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(FindElements(by, &elements));
  ASSERT_EQ(0u, elements.size());
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

void DOMElementProxy::EnsureAttributeEventuallyMatches(
    const std::string& attribute, const std::string& new_value) {
  ASSERT_TRUE(is_valid());

  const char* script = "domAutomation.waitForAttribute("
                       "domAutomation.getObject(%s), %s, %s,"
                       "domAutomation.getCallId())";
  if (!executor_->ExecuteAsyncJavaScript(
      JavaScriptPrintf(script, this->handle(), attribute, new_value))) {
    FAIL() << "Executing or parsing JavaScript failed";
  }
}

template <typename T>
bool DOMElementProxy::GetValue(const std::string& type, T* value) {
  if (!value) {
    NOTREACHED();
    return false;
  }
  if (!is_valid())
    return false;

  const char* script = "domAutomation.getValue("
                       "domAutomation.getObject(%s), %s);";
  return executor_->ExecuteJavaScriptAndGetReturn(
      JavaScriptPrintf(script, this->handle(), type), value);
}
