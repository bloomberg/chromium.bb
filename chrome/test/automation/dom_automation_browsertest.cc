// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "chrome/test/automation/javascript_execution_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// Tests the DOMAutomation framework for manipulating DOMElements within
// browser tests.
class DOMAutomationTest : public InProcessBrowserTest {
 public:
  DOMAutomationTest() {
    EnableDOMAutomation();
    JavaScriptExecutionController::set_timeout(30000);
  }

  GURL GetTestURL(const char* path) {
    std::string url_path = "files/dom_automation/";
    url_path.append(path);
    return test_server()->GetURL(url_path);
  }
};

typedef DOMElementProxy::By By;

#if defined(OS_WIN)
// See http://crbug.com/61636
#define MAYBE_FindByXPath DISABLED_FindByXPath
#define MAYBE_FindBySelectors DISABLED_FindByXPath
#define MAYBE_FindByText DISABLED_FindByText
#else
#define MAYBE_FindByXPath FindByXPath
#define MAYBE_FindBySelectors FindBySelectors
#define MAYBE_FindByText FindByText
#endif

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, MAYBE_FindByXPath) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("find_elements/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Find first element.
  DOMElementProxyRef first_div = main_doc->FindElement(By::XPath("//div"));
  ASSERT_TRUE(first_div);
  ASSERT_NO_FATAL_FAILURE(first_div->EnsureNameMatches("0"));

  // Find many elements.
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(main_doc->FindElements(By::XPath("//div"), &elements));
  ASSERT_EQ(2u, elements.size());
  for (size_t i = 0; i < elements.size(); i++) {
    ASSERT_NO_FATAL_FAILURE(elements[i]->EnsureNameMatches(
        base::UintToString(i)));
  }

  // Find 0 elements.
  ASSERT_FALSE(main_doc->FindElement(By::XPath("//nosuchtag")));
  elements.clear();
  ASSERT_TRUE(main_doc->FindElements(By::XPath("//nosuchtag"), &elements));
  elements.clear();
  ASSERT_EQ(0u, elements.size());

  // Find with invalid xpath.
  ASSERT_FALSE(main_doc->FindElement(By::XPath("'invalid'")));
  ASSERT_FALSE(main_doc->FindElement(By::XPath(" / / ")));
  ASSERT_FALSE(main_doc->FindElements(By::XPath("'invalid'"), &elements));
  ASSERT_FALSE(main_doc->FindElements(By::XPath(" / / "), &elements));

  // Find nested elements.
  int nested_count = 0;
  std::string span_name;
  DOMElementProxyRef node = main_doc->FindElement(By::XPath("/html/body/span"));
  while (node) {
    nested_count++;
    span_name.append("span");
    ASSERT_NO_FATAL_FAILURE(node->EnsureNameMatches(span_name));
    node = node->FindElement(By::XPath("./span"));
  }
  ASSERT_EQ(3, nested_count);
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, FindBySelectors) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("find_elements/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Find first element.
  DOMElementProxyRef first_myclass =
      main_doc->FindElement(By::Selectors(".myclass"));
  ASSERT_TRUE(first_myclass);
  ASSERT_NO_FATAL_FAILURE(first_myclass->EnsureNameMatches("0"));

  // Find many elements.
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(main_doc->FindElements(By::Selectors(".myclass"), &elements));
  ASSERT_EQ(2u, elements.size());
  for (size_t i = 0; i < elements.size(); i++) {
    ASSERT_NO_FATAL_FAILURE(elements[i]->EnsureNameMatches(
        base::UintToString(i)));
  }

  // Find 0 elements.
  ASSERT_FALSE(main_doc->FindElement(By::Selectors("#nosuchid")));
  elements.clear();
  ASSERT_TRUE(main_doc->FindElements(By::Selectors("#nosuchid"), &elements));
  ASSERT_EQ(0u, elements.size());

  // Find with invalid selectors.
  ASSERT_FALSE(main_doc->FindElement(By::Selectors("1#2")));
  ASSERT_FALSE(main_doc->FindElements(By::Selectors("1#2"), &elements));

  // Find nested elements.
  int nested_count = 0;
  std::string span_name;
  DOMElementProxyRef node = main_doc->FindElement(By::Selectors("span"));
  while (node) {
    nested_count++;
    span_name.append("span");
    ASSERT_NO_FATAL_FAILURE(node->EnsureNameMatches(span_name));
    node = node->FindElement(By::Selectors("span"));
  }
  ASSERT_EQ(3, nested_count);
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, FindByText) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("find_elements/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Find first element.
  DOMElementProxyRef first_text = main_doc->FindElement(By::Text("div_text"));
  ASSERT_TRUE(first_text);
  ASSERT_NO_FATAL_FAILURE(first_text->EnsureNameMatches("0"));

  // Find many elements.
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(main_doc->FindElements(By::Text("div_text"), &elements));
  ASSERT_EQ(2u, elements.size());
  for (size_t i = 0; i < elements.size(); i++) {
    ASSERT_NO_FATAL_FAILURE(elements[i]->EnsureNameMatches(
        base::UintToString(i)));
  }

  // Find 0 elements.
  ASSERT_FALSE(main_doc->FindElement(By::Text("nosuchtext")));
  elements.clear();
  ASSERT_TRUE(main_doc->FindElements(By::Text("nosuchtext"), &elements));
  ASSERT_EQ(0u, elements.size());

  // Find nested elements.
  int nested_count = 0;
  std::string span_name;
  DOMElementProxyRef node = main_doc->FindElement(By::Text("span_text"));
  while (node) {
    nested_count++;
    span_name.append("span");
    ASSERT_NO_FATAL_FAILURE(node->EnsureNameMatches(span_name));
    node = node->FindElement(By::Text("span_text"));
  }
  ASSERT_EQ(3, nested_count);

  // Find only visible text.
  DOMElementProxyRef shown_td = main_doc->FindElement(By::Text("table_text"));
  ASSERT_TRUE(shown_td);
  ASSERT_NO_FATAL_FAILURE(shown_td->EnsureNameMatches("shown"));

  // Find text in inputs.
  ASSERT_TRUE(main_doc->FindElement(By::Text("textarea_text")));
  ASSERT_TRUE(main_doc->FindElement(By::Text("input_text")));
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, WaitFor1VisibleElement) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetTestURL("wait/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  DOMElementProxyRef div =
      main_doc->WaitFor1VisibleElement(By::Selectors("div"));
  ASSERT_TRUE(div.get());
  ASSERT_NO_FATAL_FAILURE(div->EnsureInnerHTMLMatches("div_inner"));
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, WaitForElementsToDisappear) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetTestURL("wait/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  ASSERT_TRUE(main_doc->WaitForElementsToDisappear(By::Selectors("img")));
  std::vector<DOMElementProxyRef> img_elements;
  ASSERT_TRUE(main_doc->FindElements(By::Selectors("img"), &img_elements));
  ASSERT_EQ(0u, img_elements.size());
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, EnsureAttributeEventuallyMatches) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetTestURL("wait/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  DOMElementProxyRef anchor = main_doc->FindElement(By::Selectors("a"));
  ASSERT_TRUE(anchor.get());
  ASSERT_NO_FATAL_FAILURE(anchor->EnsureAttributeEventuallyMatches(
      "href", "http://www.google.com"));
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, Frames) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetTestURL("frames/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Get both frame elements.
  std::vector<DOMElementProxyRef> frame_elements;
  ASSERT_TRUE(main_doc->FindElements(By::XPath("//frame"), &frame_elements));
  ASSERT_EQ(2u, frame_elements.size());

  // Get both frames, checking their contents are correct.
  DOMElementProxyRef frame1 = frame_elements[0]->GetContentDocument();
  DOMElementProxyRef frame2 = frame_elements[1]->GetContentDocument();
  ASSERT_TRUE(frame1 && frame2);
  DOMElementProxyRef frame_div =
      frame1->FindElement(By::XPath("/html/body/div"));
  ASSERT_TRUE(frame_div);
  ASSERT_NO_FATAL_FAILURE(frame_div->EnsureInnerHTMLMatches("frame 1"));
  frame_div = frame2->FindElement(By::XPath("/html/body/div"));
  ASSERT_TRUE(frame_div);
  ASSERT_NO_FATAL_FAILURE(frame_div->EnsureInnerHTMLMatches("frame 2"));

  // Get both inner iframes, checking their contents are correct.
  DOMElementProxyRef iframe1 =
      frame1->GetDocumentFromFrame("0");
  DOMElementProxyRef iframe2 =
      frame2->GetDocumentFromFrame("0");
  ASSERT_TRUE(iframe1 && iframe2);
  frame_div = iframe1->FindElement(By::XPath("/html/body/div"));
  ASSERT_TRUE(frame_div);
  ASSERT_NO_FATAL_FAILURE(frame_div->EnsureInnerHTMLMatches("iframe 1"));
  frame_div = iframe2->FindElement(By::XPath("/html/body/div"));
  ASSERT_TRUE(frame_div);
  ASSERT_NO_FATAL_FAILURE(frame_div->EnsureInnerHTMLMatches("iframe 2"));

  // Get nested frame.
  ASSERT_EQ(iframe1.get(), main_doc->GetDocumentFromFrame("0", "0").get());
  ASSERT_EQ(iframe2.get(), main_doc->GetDocumentFromFrame("1", "0").get());
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, Events) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetTestURL("events/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Click link and make sure text changes.
  DOMElementProxyRef link = main_doc->FindElement(By::Selectors("a"));
  ASSERT_TRUE(link && link->Click());
  ASSERT_NO_FATAL_FAILURE(link->EnsureTextMatches("clicked"));

  // Click input button and make sure textfield changes.
  DOMElementProxyRef button = main_doc->FindElement(By::Selectors("#button"));
  DOMElementProxyRef textfield =
      main_doc->FindElement(By::Selectors("#textfield"));
  ASSERT_TRUE(textfield && button && button->Click());
  ASSERT_NO_FATAL_FAILURE(textfield->EnsureTextMatches("clicked"));

  // Type in the textfield.
  ASSERT_TRUE(textfield->SetText("test"));
  ASSERT_NO_FATAL_FAILURE(textfield->EnsureTextMatches("test"));

  // Type in the textarea.
  DOMElementProxyRef textarea =
      main_doc->FindElement(By::Selectors("textarea"));
  ASSERT_TRUE(textarea && textarea->Type("test"));
  ASSERT_NO_FATAL_FAILURE(textarea->EnsureTextMatches("textareatest"));
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, StringEscape) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("string_escape/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  DOMElementProxyRef textarea =
      main_doc->FindElement(By::Selectors("textarea"));
  ASSERT_TRUE(textarea);
  ASSERT_NO_FATAL_FAILURE(textarea->EnsureTextMatches(WideToUTF8(L"\u00FF")));

  const wchar_t* set_and_expect_strings[] = {
      L"\u00FF and \u00FF",
      L"\n \t \\",
      L"' \""
  };
  for (size_t i = 0; i < 3; i++) {
    ASSERT_TRUE(textarea->SetText(WideToUTF8(set_and_expect_strings[i])));
    ASSERT_NO_FATAL_FAILURE(textarea->EnsureTextMatches(
        WideToUTF8(set_and_expect_strings[i])));
  }
}

}  // namespace
