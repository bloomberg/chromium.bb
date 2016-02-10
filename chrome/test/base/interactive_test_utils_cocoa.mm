// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"

namespace ui_test_utils {

namespace {

void MoveMouseToNSViewCenterAndPress(
    NSView* view,
    ui_controls::MouseButton button,
    int state,
    const base::Closure& task) {
  NSWindow* window = [view window];
  NSScreen* screen = [window screen];
  DCHECK(screen);

  // Converts the center position of the view into the coordinates accepted
  // by SendMouseMoveNotifyWhenDone() method.
  NSRect bounds = [view bounds];
  NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  center = [view convertPoint:center toView:nil];
  center = [window convertBaseToScreen:center];
  center = NSMakePoint(center.x, [screen frame].size.height - center.y);

  ui_controls::SendMouseMoveNotifyWhenDone(
      center.x, center.y,
      base::Bind(&internal::ClickTask, button, state, task));
}

}  // namespace

bool IsViewFocused(const Browser* browser, ViewID vid) {
  NSWindow* window = browser->window()->GetNativeWindow();
  DCHECK(window);
  NSView* view = view_id_util::GetView(window, vid);
  if (!view)
    return false;

  NSResponder* firstResponder = [window firstResponder];
  if (firstResponder == static_cast<NSResponder*>(view))
    return true;

  // Handle special case for VIEW_ID_TAB_CONTAINER.  The tab container NSView
  // always transfers first responder status to its subview, so test whether
  // |firstResponder| is a descendant.
  if (vid == VIEW_ID_TAB_CONTAINER &&
      [firstResponder isKindOfClass:[NSView class]])
    return [static_cast<NSView*>(firstResponder) isDescendantOf:view];

  // Handle the special case of focusing a TextField.
  if ([firstResponder isKindOfClass:[NSTextView class]]) {
    NSView* delegate = static_cast<NSView*>([(NSTextView*)firstResponder
                                                          delegate]);
    if (delegate == view)
      return true;
  }

  return false;
}

void ClickOnView(const Browser* browser, ViewID vid) {
  NSWindow* window = browser->window()->GetNativeWindow();
  DCHECK(window);
  NSView* view = view_id_util::GetView(window, vid);
  DCHECK(view);
  MoveMouseToNSViewCenterAndPress(
      view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      base::MessageLoop::QuitWhenIdleClosure());
  content::RunMessageLoop();
}

void FocusView(const Browser* browser, ViewID vid) {
   NSWindow* window = browser->window()->GetNativeWindow();
   DCHECK(window);
   NSView* view = view_id_util::GetView(window, vid);
   DCHECK(view);
   [window makeFirstResponder:view];
 }

}  // namespace ui_test_utils
