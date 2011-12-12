// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

#if defined(OS_WIN)
#include <atlbase.h>
#include <atlcom.h>
#include "ui/base/win/atl_module.h"
#endif

using webkit_glue::WebAccessibility;

namespace {

class RendererAccessibilityBrowserTest : public InProcessBrowserTest {
 public:
  RendererAccessibilityBrowserTest() {}

  // Tell the renderer to send an accessibility tree, then wait for the
  // notification that it's been received.
  const WebAccessibility& GetWebAccessibilityTree() {
    ui_test_utils::WindowedNotificationObserver tree_updated_observer(
        content::NOTIFICATION_RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,
        content::NotificationService::AllSources());
    RenderWidgetHostView* host_view =
        browser()->GetSelectedTabContents()->GetRenderWidgetHostView();
    RenderWidgetHost* host = host_view->GetRenderWidgetHost();
    RenderViewHost* view_host = static_cast<RenderViewHost*>(host);
    view_host->set_save_accessibility_tree_for_testing(true);
    view_host->EnableRendererAccessibility();
    tree_updated_observer.Wait();
    return view_host->accessibility_tree_for_testing();
  }

  // Make sure each node in the tree has an unique id.
  void RecursiveAssertUniqueIds(
      const WebAccessibility& node, base::hash_set<int>* ids) {
    ASSERT_TRUE(ids->find(node.id) == ids->end());
    ids->insert(node.id);
    for (size_t i = 0; i < node.children.size(); i++)
      RecursiveAssertUniqueIds(node.children[i], ids);
  }

  // InProcessBrowserTest
  void SetUpInProcessBrowserTestFixture();
  void TearDownInProcessBrowserTestFixture();

 protected:
  std::string GetAttr(const WebAccessibility& node,
                      const WebAccessibility::StringAttribute attr);
  int GetIntAttr(const WebAccessibility& node,
                 const WebAccessibility::IntAttribute attr);
  bool GetBoolAttr(const WebAccessibility& node,
                   const WebAccessibility::BoolAttribute attr);
};

void RendererAccessibilityBrowserTest::SetUpInProcessBrowserTestFixture() {
#if defined(OS_WIN)
  ui::win::CreateATLModuleIfNeeded();
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
    const WebAccessibility& node,
    const WebAccessibility::StringAttribute attr) {
  std::map<WebAccessibility::StringAttribute, string16>::const_iterator iter =
      node.string_attributes.find(attr);
  if (iter != node.string_attributes.end())
    return UTF16ToUTF8(iter->second);
  else
    return "";
}

// Convenience method to get the value of a particular WebAccessibility
// node integer attribute.
int RendererAccessibilityBrowserTest::GetIntAttr(
    const WebAccessibility& node,
    const WebAccessibility::IntAttribute attr) {
  std::map<WebAccessibility::IntAttribute, int32>::const_iterator iter =
      node.int_attributes.find(attr);
  if (iter != node.int_attributes.end())
    return iter->second;
  else
    return -1;
}

// Convenience method to get the value of a particular WebAccessibility
// node boolean attribute.
bool RendererAccessibilityBrowserTest::GetBoolAttr(
    const WebAccessibility& node,
    const WebAccessibility::BoolAttribute attr) {
  std::map<WebAccessibility::BoolAttribute, bool>::const_iterator iter =
      node.bool_attributes.find(attr);
  if (iter != node.bool_attributes.end())
    return iter->second;
  else
    return false;
}

// Marked flaky per http://crbug.com/101984
IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       FLAKY_CrossPlatformWebpageAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
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
  EXPECT_EQ(WebAccessibility::ROLE_ROOT_WEB_AREA, tree.role);

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
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  ASSERT_EQ(1U, body.children.size());
  const WebAccessibility& text = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_TEXT_FIELD, text.role);
  EXPECT_STREQ(
      "input", GetAttr(text, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_EQ(0, GetIntAttr(text, WebAccessibility::ATTR_TEXT_SEL_START));
  EXPECT_EQ(0, GetIntAttr(text, WebAccessibility::ATTR_TEXT_SEL_END));
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
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  ASSERT_EQ(1U, body.children.size());
  const WebAccessibility& text = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_TEXT_FIELD, text.role);
  EXPECT_STREQ(
      "input", GetAttr(text, WebAccessibility::ATTR_HTML_TAG).c_str());
  EXPECT_EQ(0, GetIntAttr(text, WebAccessibility::ATTR_TEXT_SEL_START));
  EXPECT_EQ(13, GetIntAttr(text, WebAccessibility::ATTR_TEXT_SEL_END));
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
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

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

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformMultipleInheritanceAccessibility2) {
  // Here's another html snippet where WebKit puts the same node as a child
  // of two different parents. Instead of checking the exact output, just
  // make sure that no id is reused in the resulting tree.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<script>\n"
      "  document.writeln('<q><section></section></q><q><li>');\n"
      "  setTimeout(function() {\n"
      "    document.close();\n"
      "  }, 1);\n"
      "</script>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  base::hash_set<int> ids;
  RecursiveAssertUniqueIds(tree, &ids);
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformIframeAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html><html><body>"
      "<button>Button 1</button>"
      "<iframe src='data:text/html,"
      "<!doctype html><html><body><button>Button 2</button></body></html>"
      "'></iframe>"
      "<button>Button 3</button>"
      "</body></html>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  ASSERT_EQ(3U, body.children.size());

  const WebAccessibility& button1 = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_BUTTON, button1.role);
  EXPECT_STREQ("Button 1", UTF16ToUTF8(button1.name).c_str());

  const WebAccessibility& iframe = body.children[1];
  EXPECT_STREQ("iframe",
               GetAttr(iframe, WebAccessibility::ATTR_HTML_TAG).c_str());
  ASSERT_EQ(1U, iframe.children.size());

  const WebAccessibility& scroll_area = iframe.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_SCROLLAREA, scroll_area.role);
  ASSERT_EQ(1U, scroll_area.children.size());

  const WebAccessibility& sub_document = scroll_area.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_WEB_AREA, sub_document.role);
  ASSERT_EQ(1U, sub_document.children.size());

  const WebAccessibility& sub_body = sub_document.children[0];
  ASSERT_EQ(1U, sub_body.children.size());

  const WebAccessibility& button2 = sub_body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_BUTTON, button2.role);
  EXPECT_STREQ("Button 2", UTF16ToUTF8(button2.name).c_str());

  const WebAccessibility& button3 = body.children[2];
  EXPECT_EQ(WebAccessibility::ROLE_BUTTON, button3.role);
  EXPECT_STREQ("Button 3", UTF16ToUTF8(button3.name).c_str());
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformDuplicateChildrenAccessibility) {
  // Here's another html snippet where WebKit has a parent node containing
  // two duplicate child nodes. Instead of checking the exact output, just
  // make sure that no id is reused in the resulting tree.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<em><code ><h4 ></em>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  base::hash_set<int> ids;
  RecursiveAssertUniqueIds(tree, &ids);
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformTableSpan) {
  // +---+---+---+
  // |   1   | 2 |
  // +---+---+---+
  // | 3 |   4   |
  // +---+---+---+

  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<table border=1>"
      " <tr>"
      "  <td colspan=2>1</td><td>2</td>"
      " </tr>"
      " <tr>"
      "  <td>3</td><td colspan=2>4</td>"
      " </tr>"
      "</table>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  const WebAccessibility& tree = GetWebAccessibilityTree();
  const WebAccessibility& table = tree.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_TABLE, table.role);
  ASSERT_GE(table.children.size(), 5U);
  EXPECT_EQ(WebAccessibility::ROLE_ROW, table.children[0].role);
  EXPECT_EQ(WebAccessibility::ROLE_ROW, table.children[1].role);
  EXPECT_EQ(WebAccessibility::ROLE_COLUMN, table.children[2].role);
  EXPECT_EQ(WebAccessibility::ROLE_COLUMN, table.children[3].role);
  EXPECT_EQ(WebAccessibility::ROLE_COLUMN, table.children[4].role);
  EXPECT_EQ(3, GetIntAttr(table, WebAccessibility::ATTR_TABLE_COLUMN_COUNT));
  EXPECT_EQ(2, GetIntAttr(table, WebAccessibility::ATTR_TABLE_ROW_COUNT));

  const WebAccessibility& cell1 = table.children[0].children[0];
  const WebAccessibility& cell2 = table.children[0].children[1];
  const WebAccessibility& cell3 = table.children[1].children[0];
  const WebAccessibility& cell4 = table.children[1].children[1];

  ASSERT_EQ(6U, table.cell_ids.size());
  EXPECT_EQ(cell1.id, table.cell_ids[0]);
  EXPECT_EQ(cell1.id, table.cell_ids[1]);
  EXPECT_EQ(cell2.id, table.cell_ids[2]);
  EXPECT_EQ(cell3.id, table.cell_ids[3]);
  EXPECT_EQ(cell4.id, table.cell_ids[4]);
  EXPECT_EQ(cell4.id, table.cell_ids[5]);

  EXPECT_EQ(0, GetIntAttr(cell1,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX));
  EXPECT_EQ(0, GetIntAttr(cell1,
                          WebAccessibility::ATTR_TABLE_CELL_ROW_INDEX));
  EXPECT_EQ(2, GetIntAttr(cell1,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN));
  EXPECT_EQ(1, GetIntAttr(cell1,
                          WebAccessibility::ATTR_TABLE_CELL_ROW_SPAN));
  EXPECT_EQ(2, GetIntAttr(cell2,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX));
  EXPECT_EQ(1, GetIntAttr(cell2,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN));
  EXPECT_EQ(0, GetIntAttr(cell3,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX));
  EXPECT_EQ(1, GetIntAttr(cell3,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN));
  EXPECT_EQ(1, GetIntAttr(cell4,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX));
  EXPECT_EQ(2, GetIntAttr(cell4,
                          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN));
}

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       CrossPlatformWritableElement) {
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<div role='textbox' tabindex=0>"
      " Some text"
      "</div>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  const WebAccessibility& tree = GetWebAccessibilityTree();

  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& textbox = tree.children[0];

  EXPECT_EQ(
      true, GetBoolAttr(textbox, WebAccessibility::ATTR_CAN_SET_VALUE));
}

}  // namespace
