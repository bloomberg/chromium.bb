// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

class AccessibilityModeTest : public ContentBrowserTest {
 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  const BrowserAccessibility* FindNode(ui::AXRole role,
                                       const std::string& name) {
    const BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

 private:
  const BrowserAccessibility* FindNodeInSubtree(
      const BrowserAccessibility& node,
      ui::AXRole role,
      const std::string& name) {
    if (node.GetRole() == role &&
        node.GetStringAttribute(ui::AX_ATTR_NAME) == name)
      return &node;
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      const BrowserAccessibility* result = FindNodeInSubtree(
          *node.PlatformGetChild(i), role, name);
      if (result)
        return result;
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeOff) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  EXPECT_EQ(AccessibilityModeOff, web_contents()->GetAccessibilityMode());
  EXPECT_EQ(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeComplete) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(AccessibilityModeOff, web_contents()->GetAccessibilityMode());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_COMPLETE);
  EXPECT_EQ(ACCESSIBILITY_MODE_COMPLETE,
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  EXPECT_NE(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       AccessibilityModeWebContentsOnly) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(AccessibilityModeOff, web_contents()->GetAccessibilityMode());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY);
  EXPECT_EQ(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY,
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  // No BrowserAccessibilityManager expected for this mode.
  EXPECT_EQ(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddingModes) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY);
  EXPECT_EQ(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY,
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  EXPECT_EQ(nullptr, GetManager());

  AccessibilityNotificationWaiter waiter2(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_COMPLETE);
  EXPECT_EQ(ACCESSIBILITY_MODE_COMPLETE,
            web_contents()->GetAccessibilityMode());
  waiter2.WaitForNotification();
  EXPECT_NE(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       FullAccessibilityHasInlineTextBoxes) {
  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // we should do it with accessibility flags instead. http://crbug.com/672205
#if !defined(OS_ANDROID)
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ACCESSIBILITY_MODE_COMPLETE,
                                         ui::AX_EVENT_LOAD_COMPLETE);
  GURL url("data:text/html,<p>Para</p>");
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  const BrowserAccessibility* text = FindNode(ui::AX_ROLE_STATIC_TEXT, "Para");
  ASSERT_NE(nullptr, text);
  ASSERT_EQ(1U, text->InternalChildCount());
  BrowserAccessibility* inline_text = text->InternalGetChild(0);
  ASSERT_NE(nullptr, inline_text);
  EXPECT_EQ(ui::AX_ROLE_INLINE_TEXT_BOX, inline_text->GetRole());
#endif  // !defined(OS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       MinimalAccessibilityModeHasNoInlineTextBoxes) {
  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // we should do it with accessibility flags instead. http://crbug.com/672205
#if !defined(OS_ANDROID)
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ACCESSIBILITY_MODE_FLAG_NATIVE_APIS |
      ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS,
      ui::AX_EVENT_LOAD_COMPLETE);
  GURL url("data:text/html,<p>Para</p>");
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  const BrowserAccessibility* text = FindNode(ui::AX_ROLE_STATIC_TEXT, "Para");
  ASSERT_NE(nullptr, text);
  EXPECT_EQ(0U, text->InternalChildCount());
#endif  // !defined(OS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddScreenReaderModeFlag) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ACCESSIBILITY_MODE_FLAG_NATIVE_APIS |
      ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS,
      ui::AX_EVENT_LOAD_COMPLETE);
  GURL url("data:text/html,<input aria-label=Foo placeholder=Bar>");
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  const BrowserAccessibility* textbox = FindNode(ui::AX_ROLE_TEXT_FIELD, "Foo");
  ASSERT_NE(nullptr, textbox);
  EXPECT_FALSE(textbox->HasStringAttribute(ui::AX_ATTR_PLACEHOLDER));
  int original_id = textbox->GetId();

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), 0, ui::AX_EVENT_LAYOUT_COMPLETE);
  BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
      ACCESSIBILITY_MODE_FLAG_SCREEN_READER);
  waiter2.WaitForNotification();

  const BrowserAccessibility* textbox2 = FindNode(
      ui::AX_ROLE_TEXT_FIELD, "Foo");
  ASSERT_NE(nullptr, textbox2);
  EXPECT_TRUE(textbox2->HasStringAttribute(ui::AX_ATTR_PLACEHOLDER));
  EXPECT_NE(original_id, textbox2->GetId());
}

}  // namespace content
