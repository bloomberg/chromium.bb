// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_UTIL_H_

#include <vector>

#include "chrome/common/extensions/api/automation_internal.h"
#include "content/public/browser/ax_event_notification_details.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace extensions {

// Shared utility functions for the Automation API.
namespace automation_util {

// Dispatch events through the Automation API making any necessary conversions
// from accessibility to Automation types.
void DispatchAccessibilityEventsToAutomation(
    const std::vector<content::AXEventNotificationDetails>& details,
    content::BrowserContext* browser_context,
    const gfx::Vector2d& location_offset);

void DispatchTreeDestroyedEventToAutomation(
    int process_id,
    int routing_id,
    content::BrowserContext* browser_context);
}  // namespace automation_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_UTIL_H_
