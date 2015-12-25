// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"

namespace content {

class TouchAccessibilityBrowserTest : public ContentBrowserTest {
 public:
  TouchAccessibilityBrowserTest() {}

  DISALLOW_COPY_AND_ASSIGN(TouchAccessibilityBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TouchAccessibilityBrowserTest,
                       TouchExplorationSendsHoverEvents) {
  // Create HTML with a 7 x 5 table, each exactly 50 x 50 pixels.
  std::string html_url =
      "data:text/html,"
      "<style>"
      "  body { margin: 0; }"
      "  table { border-spacing: 0; border-collapse: collapse; }"
      "  td { width: 50px; height: 50px; padding: 0; }"
      "</style>"
      "<body>"
      "<table>";
  int cell = 0;
  for (int row = 0; row < 5; ++row) {
    html_url += "<tr>";
    for (int col = 0; col < 7; ++col) {
      html_url += "<td>" + base::IntToString(cell) + "</td>";
      ++cell;
    }
    html_url += "</tr>";
  }
  html_url += "</table></body>";

  // Navigate to the url and load the accessibility tree.
  AccessibilityNotificationWaiter waiter(
      shell(), AccessibilityModeComplete, ui::AX_EVENT_LOAD_COMPLETE);
  GURL url(html_url);
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  // Get the BrowserAccessibilityManager.
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  ASSERT_NE(nullptr, manager);

  // Loop over all of the cells in the table. For each one, send a simulated
  // touch exploration event in the center of that cell, and assert that we
  // get an accessibility hover event fired in the correct cell.
  AccessibilityNotificationWaiter waiter2(
      shell(), AccessibilityModeComplete, ui::AX_EVENT_HOVER);
  aura::Window* window = web_contents->GetContentNativeView();
  ui::EventProcessor* dispatcher = window->GetHost()->event_processor();
  for (int row = 0; row < 5; ++row) {
    for (int col = 0; col < 7; ++col) {
      std::string expected_cell_text = base::IntToString(row * 7 + col);
      VLOG(1) << "Sending event in row " << row << " col " << col
              << " with text " << expected_cell_text;

      // Send a touch exploration event to the center of the particular grid
      // cell. A touch exploration event is just a mouse move event with
      // the ui::EF_TOUCH_ACCESSIBILITY flag set.
      gfx::Rect bounds = window->GetBoundsInRootWindow();
      gfx::Point location(bounds.x() + 50 * col + 25,
                          bounds.y() + 50 * row + 25);
      int flags = ui::EF_TOUCH_ACCESSIBILITY;
      scoped_ptr<ui::Event> mouse_move_event(new ui::MouseEvent(
          ui::ET_MOUSE_MOVED, location, location, ui::EventTimeForNow(),
          flags, 0));
      ignore_result(dispatcher->OnEventFromSource(mouse_move_event.get()));

      // Wait until we get a hover event in the expected grid cell.
      // Tolerate additional events, keep looping until we get the expected one.
      std::string cell_text;
      do {
        waiter2.WaitForNotification();
        int target_id = waiter2.event_target_id();
        BrowserAccessibility* hit = manager->GetFromID(target_id);
        BrowserAccessibility* child = hit->PlatformGetChild(0);
        ASSERT_NE(nullptr, child);
        cell_text = child->GetData().GetStringAttribute(ui::AX_ATTR_NAME);
        VLOG(1) << "Got hover event in cell with text: " << cell_text;
      } while (cell_text != expected_cell_text);
    }
  }
}

}  // namespace content
