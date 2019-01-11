// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>

#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"

namespace content {

class AccessibilityAuraLinuxBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityAuraLinuxBrowserTest();
  ~AccessibilityAuraLinuxBrowserTest() override;

  static bool HasObjectWithAtkRoleFrameInAncestry(AtkObject* object) {
    while (object) {
      if (atk_object_get_role(object) == ATK_ROLE_FRAME)
        return true;
      object = atk_object_get_parent(object);
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityAuraLinuxBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, AuraLinuxBrowserAccessibleParent) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  NavigateToURL(shell(), GURL("data:text/html,"));
  waiter.WaitForNotification();

  // Get the BrowserAccessibilityManager.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  ASSERT_NE(nullptr, manager);

  auto* host_view = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ASSERT_NE(nullptr, host_view->GetNativeViewAccessible());

  AtkObject* host_view_parent =
      host_view->AccessibilityGetNativeViewAccessible();
  ASSERT_NE(nullptr, host_view_parent);
  ASSERT_TRUE(
      AccessibilityAuraLinuxBrowserTest::HasObjectWithAtkRoleFrameInAncestry(
          host_view_parent));
}

}  // namespace content
