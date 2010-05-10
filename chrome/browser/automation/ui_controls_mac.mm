// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#import <Cocoa/Cocoa.h>
#include <mach/mach_time.h>

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"

// Implementation details: We use [NSApplication sendEvent:] instead
// of [NSApplication postEvent:atStart:] so that the event gets sent
// immediately.  This lets us run the post-event task right
// immediately as well.  Unfortunately I cannot subclass NSEvent (it's
// probably a class cluster) to allow other easy answers.  For
// example, if I could subclass NSEvent, I could run the Task in it's
// dealloc routine (which necessarily happens after the event is
// dispatched).  Unlike Linux, Mac does not have message loop
// observer/notification.  Unlike windows, I cannot post non-events
// into the event queue.  (I can post other kinds of tasks but can't
// guarantee their order with regards to events).

namespace {

// From
// http://stackoverflow.com/questions/1597383/cgeventtimestamp-to-nsdate
// Which credits Apple sample code for this routine.
uint64_t UpTimeInNanoseconds(void) {
  uint64_t time;
  uint64_t timeNano;
  static mach_timebase_info_data_t sTimebaseInfo;

  time = mach_absolute_time();

  // Convert to nanoseconds.

  // If this is the first time we've run, get the timebase.
  // We can use denom == 0 to indicate that sTimebaseInfo is
  // uninitialised because it makes no sense to have a zero
  // denominator is a fraction.
  if (sTimebaseInfo.denom == 0) {
    (void) mach_timebase_info(&sTimebaseInfo);
  }

  // This could overflow; for testing needs we probably don't care.
  timeNano = time * sTimebaseInfo.numer / sTimebaseInfo.denom;
  return timeNano;
}

NSTimeInterval TimeIntervalSinceSystemStartup() {
  return UpTimeInNanoseconds() / 1000000000.0;
}

}  // anonymous namespace


namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  base::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  return SendKeyPressNotifyWhenDone(window, key,
                                    control, shift, alt, command,
                                    NULL);
}

// Win and Linux implement a SendKeyPress() this as a
// SendKeyPressAndRelease(), so we should as well (despite the name).
//
// TODO(jrg): handle "characters" better (e.g. apply shift?)
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                base::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                Task* task) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  NSUInteger flags = 0;
  if (control)
    flags |= NSControlKeyMask;
  if (shift)
    flags |= NSShiftKeyMask;
  if (alt)
    flags |= NSAlternateKeyMask;
  if (command)
    flags |= NSCommandKeyMask;
  unsigned char keycode = key;
  NSString* charactersIgnoringModifiers = [[[NSString alloc]
                                             initWithBytes:&keycode
                                                    length:1
                                                  encoding:NSUTF8StringEncoding]
                                            autorelease];
  NSString* characters = charactersIgnoringModifiers;

  // For events other than mouse moved, [event locationInWindow] is
  // UNDEFINED if the event is not NSMouseMoved.  Thus, the (0,0)
  // locaiton should be fine.
  // First a key down...
  NSEvent* event =
      [NSEvent keyEventWithType:NSKeyDown
                       location:NSMakePoint(0,0)
                  modifierFlags:flags
                      timestamp:TimeIntervalSinceSystemStartup()
                   windowNumber:[window windowNumber]
                        context:nil
                     characters:characters
    charactersIgnoringModifiers:charactersIgnoringModifiers
                      isARepeat:NO
                        keyCode:key];
  [[NSApplication sharedApplication] sendEvent:event];
  // Then a key up.
  event =
      [NSEvent keyEventWithType:NSKeyUp
                       location:NSMakePoint(0,0)
                  modifierFlags:flags
                      timestamp:TimeIntervalSinceSystemStartup()
                   windowNumber:[window windowNumber]
                        context:nil
                     characters:characters
    charactersIgnoringModifiers:charactersIgnoringModifiers
                      isARepeat:NO
                        keyCode:key];
  [[NSApplication sharedApplication] sendEvent:event];

  if (task)
    MessageLoop::current()->PostTask(FROM_HERE, task);
  return true;
}

bool SendMouseMove(long x, long y) {
  return SendMouseMoveNotifyWhenDone(x, y, NULL);
}

// Input position is in screen coordinates.  However, NSMouseMoved
// events require them window-relative, so we adjust.  We *DO* flip
// the coordinate space, so input events can be the same for all
// platforms.  E.g. (0,0) is upper-left.
bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  NSWindow* window = [[NSApplication sharedApplication] keyWindow];
  CGFloat screenHeight = [[NSScreen mainScreen] frame].size.height;
  NSPoint pointInWindow = NSMakePoint(x, screenHeight - y);  // flip!
  if (window)
    pointInWindow = [window convertScreenToBase:pointInWindow];
  NSTimeInterval timestamp = TimeIntervalSinceSystemStartup();

  NSEvent* event =
      [NSEvent mouseEventWithType:NSMouseMoved
                         location:pointInWindow
                    modifierFlags:0
                        timestamp:timestamp
                     windowNumber:[window windowNumber]
                          context:nil
                      eventNumber:0
                       clickCount:0
                         pressure:0.0];
  [[NSApplication sharedApplication] postEvent:event atStart:NO];
  if (task)
    MessageLoop::current()->PostTask(FROM_HERE, task);
  return true;
}

bool SendMouseEvents(MouseButton type, int state) {
  return SendMouseEventsNotifyWhenDone(type, state, NULL);
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task) {
  // On windows it appears state can be (UP|DOWN).  It is unclear if
  // that'll happen here but prepare for it just in case.
  if (state == (UP|DOWN)) {
    return (SendMouseEventsNotifyWhenDone(type, DOWN, NULL) &&
            SendMouseEventsNotifyWhenDone(type, UP, task));
  }

  NSEventType etype = 0;
  if (type == LEFT) {
    if (state == UP) {
      etype = NSLeftMouseUp;
    } else {
      etype = NSLeftMouseDown;
    }
  } else if (type == MIDDLE) {
    if (state == UP) {
      etype = NSOtherMouseUp;
    } else {
      etype = NSOtherMouseDown;
    }
  } else if (type == RIGHT) {
    if (state == UP) {
      etype = NSRightMouseUp;
    } else {
      etype = NSRightMouseDown;
    }
  } else {
    return false;
  }
  NSWindow* window = [[NSApplication sharedApplication] keyWindow];
  NSPoint location = [NSEvent mouseLocation];
  NSPoint pointInWindow = location;
  if (window)
    pointInWindow = [window convertScreenToBase:pointInWindow];

  NSEvent* event =
      [NSEvent mouseEventWithType:etype
                         location:pointInWindow
                    modifierFlags:0
                        timestamp:TimeIntervalSinceSystemStartup()
                     windowNumber:[window windowNumber]
                          context:nil
                      eventNumber:0
                       clickCount:0
                         pressure:0.0];
  [[NSApplication sharedApplication] sendEvent:event];
  if (task)
    MessageLoop::current()->PostTask(FROM_HERE, task);
  return true;
}

bool SendMouseClick(MouseButton type) {
 return SendMouseEventsNotifyWhenDone(type, UP|DOWN, NULL);
}

// This appears to only be used by a function in test/ui_test_utils.h:
// ui_test_utils::ClickOnView().  That is not implemented on Mac, so
// we don't need to implement MoveMouseToCenterAndPress().  I've
// suggested an implementation of ClickOnView() which would call Cocoa
// directly and not need this indirection, so this may not be needed,
// ever.
void MoveMouseToCenterAndPress(
    NSWindow* window,
    MouseButton button,
    int state,
    Task* task) {
  NOTIMPLEMENTED();
}

}  // ui_controls
