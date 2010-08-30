// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

using webkit_glue::WebAccessibility;

namespace {

class RendererAccessibilityBrowserTest : public InProcessBrowserTest {
 public:
  RendererAccessibilityBrowserTest() {}

 protected:
  const char *GetAttr(const WebAccessibility& node,
                      const WebAccessibility::Attribute attr);
};

// Convenience method to get the value of a particular WebAccessibility
// node attribute as a UTF-8 const char*.
const char *RendererAccessibilityBrowserTest::GetAttr(
    const WebAccessibility& node, const WebAccessibility::Attribute attr) {
  std::map<int32, string16>::const_iterator iter = node.attributes.find(attr);
  if (iter != node.attributes.end())
    return UTF16ToUTF8(iter->second).c_str();
  else
    return "";
}

#if defined(OS_WIN)
// http://crbug.com/53853
#define MAYBE_TestCrossPlatformAccessibilityTree \
    DISABLED_TestCrossPlatformAccessibilityTree
#else
#define MAYBE_TestCrossPlatformAccessibilityTree \
    TestCrossPlatformAccessibilityTree
#endif

IN_PROC_BROWSER_TEST_F(RendererAccessibilityBrowserTest,
                       MAYBE_TestCrossPlatformAccessibilityTree) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>";
  GURL url(url_str);
  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);

  // Tell the renderer to send an accessibility tree, then wait for the
  // notification that it's been received.
  RenderWidgetHostView* host_view =
      browser()->GetSelectedTabContents()->GetRenderWidgetHostView();
  RenderWidgetHost* host = host_view->GetRenderWidgetHost();
  RenderViewHost* view_host = static_cast<RenderViewHost*>(host);
  view_host->set_save_accessibility_tree_for_testing(true);
  host->Send(new ViewMsg_GetAccessibilityTree(host->routing_id()));
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check properties of the root element of the tree.
  const WebAccessibility& tree = view_host->accessibility_tree();
  EXPECT_STREQ(url_str, GetAttr(tree, WebAccessibility::ATTR_DOC_URL));
  EXPECT_STREQ("Accessibility Test",
               GetAttr(tree, WebAccessibility::ATTR_DOC_TITLE));
  EXPECT_STREQ("html", GetAttr(tree, WebAccessibility::ATTR_DOC_DOCTYPE));
  EXPECT_STREQ("text/html", GetAttr(tree, WebAccessibility::ATTR_DOC_MIMETYPE));
  EXPECT_EQ(WebAccessibility::ROLE_WEB_AREA, tree.role);

  // Check properites of the BODY element.
  ASSERT_EQ(1U, tree.children.size());
  const WebAccessibility& body = tree.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_GROUP, body.role);
  EXPECT_STREQ("BODY", GetAttr(body, WebAccessibility::ATTR_HTML_TAG));
  EXPECT_STREQ("block", GetAttr(body, WebAccessibility::ATTR_DISPLAY));

  // Check properties of the two children of the BODY element.
  ASSERT_EQ(2U, body.children.size());

  const WebAccessibility& button = body.children[0];
  EXPECT_EQ(WebAccessibility::ROLE_BUTTON, button.role);
  EXPECT_STREQ("INPUT", GetAttr(button, WebAccessibility::ATTR_HTML_TAG));
  EXPECT_STREQ("push", UTF16ToUTF8(button.name).c_str());
  EXPECT_STREQ("inline-block", GetAttr(button, WebAccessibility::ATTR_DISPLAY));
  ASSERT_EQ(2U, button.html_attributes.size());
  EXPECT_STREQ("type", UTF16ToUTF8(button.html_attributes[0].first).c_str());
  EXPECT_STREQ("button", UTF16ToUTF8(button.html_attributes[0].second).c_str());
  EXPECT_STREQ("value", UTF16ToUTF8(button.html_attributes[1].first).c_str());
  EXPECT_STREQ("push", UTF16ToUTF8(button.html_attributes[1].second).c_str());

  const WebAccessibility& checkbox = body.children[1];
  EXPECT_EQ(WebAccessibility::ROLE_CHECKBOX, checkbox.role);
  EXPECT_STREQ("INPUT", GetAttr(checkbox, WebAccessibility::ATTR_HTML_TAG));
  EXPECT_STREQ("inline-block",
               GetAttr(checkbox, WebAccessibility::ATTR_DISPLAY));
  ASSERT_EQ(1U, checkbox.html_attributes.size());
  EXPECT_STREQ("type",
               UTF16ToUTF8(checkbox.html_attributes[0].first).c_str());
  EXPECT_STREQ("checkbox",
               UTF16ToUTF8(checkbox.html_attributes[0].second).c_str());
}

}  // namespace
