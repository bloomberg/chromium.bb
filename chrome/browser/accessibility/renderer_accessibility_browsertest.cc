// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"

#if defined(OS_WIN)
#include <atlbase.h>
#include <atlcom.h>
#endif

using webkit_glue::WebAccessibility;

namespace {

class RendererAccessibilityBrowserTest : public InProcessBrowserTest {
 public:
  RendererAccessibilityBrowserTest() {}

  // Tell the renderer to send an accessibility tree, then wait for the
  // notification that it's been received.
  const WebAccessibility& GetWebAccessibilityTree() {
    RenderWidgetHostView* host_view =
        browser()->GetSelectedTabContents()->GetRenderWidgetHostView();
    RenderWidgetHost* host = host_view->GetRenderWidgetHost();
    RenderViewHost* view_host = static_cast<RenderViewHost*>(host);
    view_host->set_save_accessibility_tree_for_testing(true);
    view_host->EnableRendererAccessibility();
    ui_test_utils::WaitForNotification(
        NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);
    return view_host->accessibility_tree();
  }

  // InProcessBrowserTest
  void SetUpInProcessBrowserTestFixture();
  void TearDownInProcessBrowserTestFixture();

 protected:
  std::string GetAttr(const WebAccessibility& node,
                      const WebAccessibility::Attribute attr);
};

void RendererAccessibilityBrowserTest::SetUpInProcessBrowserTestFixture() {
#if defined(OS_WIN)
  // ATL needs a pointer to a COM module.
  static CComModule module;
  _pAtlModule = &module;

  // Make sure COM is initialized for this thread; it's safe to call twice.
  ::CoInitialize(NULL);
#endif
}

void RendererAccessibilityBrowserTest::TearDownInProcessBrowserTestFixture() {
#if defined(OS_WIN)
  ::CoUninitialize();
#endif
}

// Convenience method to get the value of a particular WebAccessibility
// node attribute as a UTF-8 const char*.
std::string RendererAccessibilityBrowserTest::GetAttr(
    const WebAccessibility& node, const WebAccessibility::Attribute attr) {
  std::map<int32, string16>::const_iterator iter = node.attributes.find(attr);
  if (iter != node.attributes.end())
    return UTF16ToUTF8(iter->second);
  else
    return "";
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformWebpageAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  const WebAccessibility& tree = GetWebAccessibilityTree();

  // Check properties of the root element of the tree.
  EXPECT_STREQ(url_str, GetAttr(tree, WebAccessibility::ATTR_DOC_URL).c_str());
  EXPECT_STREQ(
      "Accessibility Test",
      GetAttr(tree, WebAccessibility::ATTR_DOC_TITLE).c_str());
  EXPECT_STREQ(
      "html", GetAttr(tree, WebAccessibility::ATTR_DOC_DOCTYPE).c_str());
  EXPECT_STREQ(
      "text/html", GetAttr(tree, WebAccessibility::ATTR_DOC_MIMETYPE).c_str());
  EXPECT_STREQ("Accessibility Test", UTF16ToUTF8(tree.name).c_str());
  EXPECT_EQ(WebAccessibility::ROLE_WEB_AREA, tree.role);

  // Check properites of the BODY element.
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_GROUP, body.role);
  EXPECT_STREQ("body", GetAttr(body, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_STREQ("block", GetAttr(body, WebAccessibility::ATTR_DISPLAY).c_str());

  // Check properties of the two children of the BODY element.
  ASSERT_EQ(2U, body.children.size());

  const WebAccessibility& button = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_BUTTON, button.role);
  EXPECT_STREQ(
      "input", GetAttr(button, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_STREQ("push", UTF16ToUTF8(button.name).c_str());
  EXPECT_STREQ(
      "inline-block", GetAttr(button, WebAccessibility::ATTR_DISPLAY).c_str());
  ASSERT_EQ(2U, button.html_attributes.size());
  EXPECT_STREQ("type", UTF16ToUTF8(button.html_attributes[0].first).c_str());
  EXPECT_STREQ("button", UTF16ToUTF8(button.html_attributes[0].second).c_str());
  EXPECT_STREQ("value", UTF16ToUTF8(button.html_attributes[1].first).c_str());
  EXPECT_STREQ("push", UTF16ToUTF8(button.html_attributes[1].second).c_str());

  const WebAccessibility& checkbox = body.children[1];
  EXPECT_EQ(WebAccessibility::ROLE_CHECKBOX, checkbox.role);
  EXPECT_STREQ(
      "input", GetAttr(checkbox, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_STREQ(
      "inline-block",
      GetAttr(checkbox, WebAccessibility::ATTR_DISPLAY).c_str());
  ASSERT_EQ(1U, checkbox.html_attributes.size());
  EXPECT_STREQ(
      "type", UTF16ToUTF8(checkbox.html_attributes[0].first).c_str());
  EXPECT_STREQ(
    "checkbox", UTF16ToUTF8(checkbox.html_attributes[0].second).c_str());
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformUnselectedEditableTextAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<body>"
      "<input value=\"Hello, world.\"/>"
      "</body></html>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  ASSERT_EQ(1U, body.children.size());
  const WebAccessibility& text = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_TEXT_FIELD, text.role);
  EXPECT_STREQ(
      "input", GetAttr(text, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_STREQ(
      "0", GetAttr(text, WebAccessibility::ATTR_TEXT_SEL_START).c_str());
  EXPECT_STREQ(
      "0", GetAttr(text, WebAccessibility::ATTR_TEXT_SEL_END).c_str());
  EXPECT_STREQ("Hello, world.", UTF16ToUTF8(text.value).c_str());

  // TODO(dmazzoni): as soon as more accessibility code is cross-platform,
  // this code should test that the accessible info is dynamically updated
  // if the selection or value changes.
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformSelectedEditableTextAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<body onload=\"document.body.children[0].select();\">"
      "<input value=\"Hello, world.\"/>"
      "</body></html>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  ASSERT_EQ(1U, body.children.size());
  const WebAccessibility& text = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_TEXT_FIELD, text.role);
  EXPECT_STREQ(
      "input", GetAttr(text, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_STREQ(
      "0", GetAttr(text, WebAccessibility::ATTR_TEXT_SEL_START).c_str());
  EXPECT_STREQ(
      "13", GetAttr(text, WebAccessibility::ATTR_TEXT_SEL_END).c_str());
  EXPECT_STREQ("Hello, world.", UTF16ToUTF8(text.value).c_str());
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformMultipleInheritanceAccessibility) {
  // In a WebKit accessibility render tree for a table, each cell is a
  // child of both a row and a column, so it appears to use multiple
  // inheritance. Make sure that the WebAccessibilityObject tree only
  // keeps one copy of each cell, and uses an indirect child id for the
  // additional reference to it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<table border=1><tr><td>1</td><td>2</td></tr></table>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& table = tree.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_TABLE, table.role);
  const WebAccessibility& row = table.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_ROW, row.role);
  const WebAccessibility& cell1 = row.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_CELL, cell1.role);
  const WebAccessibility& cell2 = row.children[1];
  EXPECT_EQ(WebAccessibility::ROLE_CELL, cell2.role);
  const WebAccessibility& column1 = table.children[1];
  EXPECT_EQ(WebAccessibility::ROLE_COLUMN, column1.role);
  EXPECT_EQ(0U, column1.children.size());
  EXPECT_EQ(1U, column1.indirect_child_ids.size());
  EXPECT_EQ(cell1.id, column1.indirect_child_ids[0]);
  const WebAccessibility& column2 = table.children[2];
  EXPECT_EQ(WebAccessibility::ROLE_COLUMN, column2.role);
  EXPECT_EQ(0U, column2.children.size());
  EXPECT_EQ(1U, column2.indirect_child_ids.size());
  EXPECT_EQ(cell2.id, column2.indirect_child_ids[0]);
}

}  // namespace
