// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_

#include <string>

namespace content {

class BrowserAccessibility;
class WebContents;

// Search recursively through the accessibility tree rooted at |node|
// and return true if any descendant (including in child frames) has
// a name (ui::AX_ATTR_NAME) that matches |name| exactly.
bool AccessibilityTreeContainsNodeWithName(BrowserAccessibility* node,
                                           const std::string& name);

// This is intended to be a robust way to assert that the accessibility
// tree eventually gets into the correct state, without worrying about
// the exact ordering of events received while getting there.
//
// Searches the accessibility tree to see if any node's accessible name
// is equal to the given name. If not, sets up a notification waiter
// that listens for any accessibility event in any frame, and checks again
// after each event. Keeps looping until the text is found (or the
// test times out).
void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   const std::string& name);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
