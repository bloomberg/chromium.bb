// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "ui/base/test/ui_controls.h"

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
      base::MessageLoop::QuitClosure());
  content::RunMessageLoop();
}

void FocusView(const Browser* browser, ViewID vid) {
   NSWindow* window = browser->window()->GetNativeWindow();
   DCHECK(window);
   NSView* view = view_id_util::GetView(window, vid);
   DCHECK(view);
   [window makeFirstResponder:view];
 }

void HideNativeWindow(gfx::NativeWindow window) {
  [window orderOut:nil];
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  // Make sure an unbundled program can get the input focus.
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  TransformProcessType(&psn,kProcessTransformToForegroundApplication);
  SetFrontProcess(&psn);

  [window makeKeyAndOrderFront:nil];
  return true;
}

}  // namespace ui_test_utils
