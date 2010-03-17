// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "chrome/browser/browser.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

class DOMAutomationTest : public InProcessBrowserTest {
 public:
  DOMAutomationTest() {
    EnableDOMAutomation();
  }

  GURL GetTestURL(const char* path) {
    std::string url("http://localhost:1337/files/dom_automation/");
    url.append(path);
    return GURL(url);
  }
};

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, FindByXPath) {
  StartHTTPServer();
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("find_elements/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Find first element.
  DOMElementProxyRef first_div = main_doc->FindByXPath("//div");
  ASSERT_TRUE(first_div);
  ASSERT_NO_FATAL_FAILURE(first_div->EnsureNameMatches("0"));

  // Find many elements.
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(main_doc->FindByXPath("//div", &elements));
  ASSERT_EQ(2u, elements.size());
  for (size_t i = 0; i < elements.size(); i++)
    ASSERT_NO_FATAL_FAILURE(elements[i]->EnsureNameMatches(UintToString(i)));

  // Find 0 elements.
  ASSERT_FALSE(main_doc->FindByXPath("//nosuchtag"));
  elements.clear();
  ASSERT_TRUE(main_doc->FindByXPath("//nosuchtag", &elements));
  elements.clear();
  ASSERT_EQ(0u, elements.size());

  // Find with invalid xpath.
  ASSERT_FALSE(main_doc->FindByXPath("'invalid'"));
  ASSERT_FALSE(main_doc->FindByXPath(" / / "));
  ASSERT_FALSE(main_doc->FindByXPath("'invalid'", &elements));
  ASSERT_FALSE(main_doc->FindByXPath(" / / ", &elements));

  // Find nested elements.
  int nested_count = 0;
  std::string span_name;
  DOMElementProxyRef node = main_doc->FindByXPath("/html/body/span");
  while (node) {
    nested_count++;
    span_name.append("span");
    ASSERT_NO_FATAL_FAILURE(node->EnsureNameMatches(span_name));
    node = node->FindByXPath("./span");
  }
  ASSERT_EQ(3, nested_count);
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, FindBySelectors) {
  StartHTTPServer();
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("find_elements/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Find first element.
  DOMElementProxyRef first_myclass =
      main_doc->FindBySelectors(".myclass");
  ASSERT_TRUE(first_myclass);
  ASSERT_NO_FATAL_FAILURE(first_myclass->EnsureNameMatches("0"));

  // Find many elements.
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(main_doc->FindBySelectors(".myclass", &elements));
  ASSERT_EQ(2u, elements.size());
  for (size_t i = 0; i < elements.size(); i++)
    ASSERT_NO_FATAL_FAILURE(elements[i]->EnsureNameMatches(UintToString(i)));

  // Find 0 elements.
  ASSERT_FALSE(main_doc->FindBySelectors("#nosuchid"));
  elements.clear();
  ASSERT_TRUE(main_doc->FindBySelectors("#nosuchid", &elements));
  ASSERT_EQ(0u, elements.size());

  // Find with invalid selectors.
  ASSERT_FALSE(main_doc->FindBySelectors("1#2"));
  ASSERT_FALSE(main_doc->FindBySelectors("1#2", &elements));

  // Find nested elements.
  int nested_count = 0;
  std::string span_name;
  DOMElementProxyRef node = main_doc->FindBySelectors("span");
  while (node) {
    nested_count++;
    span_name.append("span");
    ASSERT_NO_FATAL_FAILURE(node->EnsureNameMatches(span_name));
    node = node->FindBySelectors("span");
  }
  ASSERT_EQ(3, nested_count);
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, FindByText) {
  StartHTTPServer();
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("find_elements/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Find first element.
  DOMElementProxyRef first_text = main_doc->FindByText("div_text");
  ASSERT_TRUE(first_text);
  ASSERT_NO_FATAL_FAILURE(first_text->EnsureNameMatches("0"));

  // Find many elements.
  std::vector<DOMElementProxyRef> elements;
  ASSERT_TRUE(main_doc->FindByText("div_text", &elements));
  ASSERT_EQ(2u, elements.size());
  for (size_t i = 0; i < elements.size(); i++)
    ASSERT_NO_FATAL_FAILURE(elements[i]->EnsureNameMatches(UintToString(i)));

  // Find 0 elements.
  ASSERT_FALSE(main_doc->FindByText("nosuchtext"));
  elements.clear();
  ASSERT_TRUE(main_doc->FindByText("nosuchtext", &elements));
  ASSERT_EQ(0u, elements.size());

  // Find nested elements.
  int nested_count = 0;
  std::string span_name;
  DOMElementProxyRef node = main_doc->FindByText("span_text");
  while (node) {
    nested_count++;
    span_name.append("span");
    ASSERT_NO_FATAL_FAILURE(node->EnsureNameMatches(span_name));
    node = node->FindByText("span_text");
  }
  ASSERT_EQ(3, nested_count);

  // Find only visible text.
  DOMElementProxyRef shown_td = main_doc->FindByText("table_text");
  ASSERT_TRUE(shown_td);
  ASSERT_NO_FATAL_FAILURE(shown_td->EnsureNameMatches("shown"));

  // Find text in inputs.
  ASSERT_TRUE(main_doc->FindByText("textarea_text"));
  ASSERT_TRUE(main_doc->FindByText("input_text"));
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, Frames) {
  StartHTTPServer();
  ui_test_utils::NavigateToURL(browser(), GetTestURL("frames/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Get both frame elements.
  std::vector<DOMElementProxyRef> frame_elements;
  ASSERT_TRUE(main_doc->FindByXPath("//frame", &frame_elements));
  ASSERT_EQ(2u, frame_elements.size());

  // Get both frames, checking their contents are correct.
  DOMElementProxyRef frame1 = frame_elements[0]->GetContentDocument();
  DOMElementProxyRef frame2 = frame_elements[1]->GetContentDocument();
  ASSERT_TRUE(frame1 && frame2);
  ASSERT_NO_FATAL_FAILURE(frame1->FindByXPath("/html/body/div")->
                          EnsureInnerHTMLMatches("frame 1"));
  ASSERT_NO_FATAL_FAILURE(frame2->FindByXPath("/html/body/div")->
                          EnsureInnerHTMLMatches("frame 2"));

  // Get both inner iframes, checking their contents are correct.
  DOMElementProxyRef iframe1 =
      frame1->GetDocumentFromFrame("0");
  DOMElementProxyRef iframe2 =
      frame2->GetDocumentFromFrame("0");
  ASSERT_TRUE(iframe1 && iframe2);
  ASSERT_NO_FATAL_FAILURE(iframe1->FindByXPath("/html/body/div")->
                          EnsureInnerHTMLMatches("iframe 1"));
  ASSERT_NO_FATAL_FAILURE(iframe2->FindByXPath("/html/body/div")->
                          EnsureInnerHTMLMatches("iframe 2"));

  // Get nested frame.
  ASSERT_EQ(iframe1.get(), main_doc->GetDocumentFromFrame("0", "0").get());
  ASSERT_EQ(iframe2.get(), main_doc->GetDocumentFromFrame("1", "0").get());
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, Events) {
  StartHTTPServer();
  ui_test_utils::NavigateToURL(browser(), GetTestURL("events/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  // Click link and make sure text changes.
  DOMElementProxyRef link = main_doc->FindBySelectors("a");
  ASSERT_TRUE(link && link->Click());
  ASSERT_NO_FATAL_FAILURE(link->EnsureTextMatches("clicked"));

  // Click input button and make sure textfield changes.
  DOMElementProxyRef button = main_doc->FindBySelectors("#button");
  DOMElementProxyRef textfield = main_doc->FindBySelectors("#textfield");
  ASSERT_TRUE(textfield && button && button->Click());
  ASSERT_NO_FATAL_FAILURE(textfield->EnsureTextMatches("clicked"));

  // Type in the textfield.
  ASSERT_TRUE(textfield->SetText("test"));
  ASSERT_NO_FATAL_FAILURE(textfield->EnsureTextMatches("test"));

  // Type in the textarea.
  DOMElementProxyRef textarea = main_doc->FindBySelectors("textarea");
  ASSERT_TRUE(textarea && textarea->Type("test"));
  ASSERT_NO_FATAL_FAILURE(textarea->EnsureTextMatches("textareatest"));
}

IN_PROC_BROWSER_TEST_F(DOMAutomationTest, StringEscape) {
  StartHTTPServer();
  ui_test_utils::NavigateToURL(browser(),
                               GetTestURL("string_escape/test.html"));
  DOMElementProxyRef main_doc = ui_test_utils::GetActiveDOMDocument(browser());

  DOMElementProxyRef textarea = main_doc->FindBySelectors("textarea");
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
