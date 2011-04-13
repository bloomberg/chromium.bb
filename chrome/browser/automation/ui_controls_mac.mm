// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#import <Cocoa/Cocoa.h>
#include <mach/mach_time.h>
#include <vector>

#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls_internal.h"
#include "content/browser/browser_thread.h"
#include "ui/base/keycodes/keyboard_code_conversion_mac.h"

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

// But [NSApplication sendEvent:] causes a problem when sending mouse click
// events. Because in order to handle mouse drag, when processing a mouse
// click event, the application may want to retrieve the next event
// synchronously by calling NSApplication's nextEventMatchingMask method.
// In this case, [NSApplication sendEvent:] causes deadlock.
// So we need to use [NSApplication postEvent:atStart:] for mouse click
// events. In order to notify the caller correctly after all events has been
// processed, we setup a task to watch for the event queue time to time and
// notify the caller as soon as there is no event in the queue.
//
// TODO(suzhe):
// 1. Investigate why using [NSApplication postEvent:atStart:] for keyboard
//    events causes BrowserKeyEventsTest.CommandKeyEvents to fail.
//    See http://crbug.com/49270
// 2. On OSX 10.6, [NSEvent addLocalMonitorForEventsMatchingMask:handler:] may
//    be used, so that we don't need to poll the event queue time to time.

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

// Creates and returns an autoreleased key event.
NSEvent* SynthesizeKeyEvent(NSWindow* window,
                            bool keyDown,
                            ui::KeyboardCode keycode,
                            NSUInteger flags) {
  unichar character;
  unichar characterIgnoringModifiers;
  int macKeycode = ui::MacKeyCodeForWindowsKeyCode(
      keycode, flags, &character, &characterIgnoringModifiers);

  if (macKeycode < 0)
    return nil;

  NSString* charactersIgnoringModifiers =
      [[[NSString alloc] initWithCharacters:&characterIgnoringModifiers
                                     length:1]
        autorelease];
  NSString* characters =
      [[[NSString alloc] initWithCharacters:&character length:1] autorelease];

  NSEventType type = (keyDown ? NSKeyDown : NSKeyUp);

  // Modifier keys generate NSFlagsChanged event rather than
  // NSKeyDown/NSKeyUp events.
  if (keycode == ui::VKEY_CONTROL || keycode == ui::VKEY_SHIFT ||
      keycode == ui::VKEY_MENU || keycode == ui::VKEY_COMMAND)
    type = NSFlagsChanged;

  // For events other than mouse moved, [event locationInWindow] is
  // UNDEFINED if the event is not NSMouseMoved.  Thus, the (0,0)
  // location should be fine.
  NSEvent* event =
      [NSEvent keyEventWithType:type
                       location:NSMakePoint(0, 0)
                  modifierFlags:flags
                      timestamp:TimeIntervalSinceSystemStartup()
                   windowNumber:[window windowNumber]
                        context:nil
                     characters:characters
    charactersIgnoringModifiers:charactersIgnoringModifiers
                      isARepeat:NO
                        keyCode:(unsigned short)macKeycode];

  return event;
}

// Creates the proper sequence of autoreleased key events for a key down + up.
void SynthesizeKeyEventsSequence(NSWindow* window,
                                 ui::KeyboardCode keycode,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command,
                                 std::vector<NSEvent*>* events) {
  NSEvent* event = nil;
  NSUInteger flags = 0;
  if (control) {
    flags |= NSControlKeyMask;
    event = SynthesizeKeyEvent(window, true, ui::VKEY_CONTROL, flags);
    DCHECK(event);
    events->push_back(event);
  }
  if (shift) {
    flags |= NSShiftKeyMask;
    event = SynthesizeKeyEvent(window, true, ui::VKEY_SHIFT, flags);
    DCHECK(event);
    events->push_back(event);
  }
  if (alt) {
    flags |= NSAlternateKeyMask;
    event = SynthesizeKeyEvent(window, true, ui::VKEY_MENU, flags);
    DCHECK(event);
    events->push_back(event);
  }
  if (command) {
    flags |= NSCommandKeyMask;
    event = SynthesizeKeyEvent(window, true, ui::VKEY_COMMAND, flags);
    DCHECK(event);
    events->push_back(event);
  }

  event = SynthesizeKeyEvent(window, true, keycode, flags);
  DCHECK(event);
  events->push_back(event);
  event = SynthesizeKeyEvent(window, false, keycode, flags);
  DCHECK(event);
  events->push_back(event);

  if (command) {
    flags &= ~NSCommandKeyMask;
    event = SynthesizeKeyEvent(window, false, ui::VKEY_COMMAND, flags);
    DCHECK(event);
    events->push_back(event);
  }
  if (alt) {
    flags &= ~NSAlternateKeyMask;
    event = SynthesizeKeyEvent(window, false, ui::VKEY_MENU, flags);
    DCHECK(event);
    events->push_back(event);
  }
  if (shift) {
    flags &= ~NSShiftKeyMask;
    event = SynthesizeKeyEvent(window, false, ui::VKEY_SHIFT, flags);
    DCHECK(event);
    events->push_back(event);
  }
  if (control) {
    flags &= ~NSControlKeyMask;
    event = SynthesizeKeyEvent(window, false, ui::VKEY_CONTROL, flags);
    DCHECK(event);
    events->push_back(event);
  }
}

// A task class to watch for the event queue. The specific task will be fired
// when there is no more event in the queue.
class EventQueueWatcher : public Task {
 public:
  EventQueueWatcher(Task* task) : task_(task) {}

  virtual ~EventQueueWatcher() {}

  virtual void Run() {
    NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                        untilDate:nil
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:NO];
    // If there is still event in the queue, then we need to check again.
    if (event)
      MessageLoop::current()->PostTask(FROM_HERE, new EventQueueWatcher(task_));
    else
      MessageLoop::current()->PostTask(FROM_HERE, task_);
  }

 private:
  Task* task_;
};

// Stores the current mouse location on the screen. So that we can use it
// when firing keyboard and mouse click events.
NSPoint g_mouse_location = { 0, 0 };

}  // anonymous namespace


namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
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
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<NSEvent*> events;
  SynthesizeKeyEventsSequence(
      window, key, control, shift, alt, command, &events);

  // TODO(suzhe): Using [NSApplication postEvent:atStart:] here causes
  // BrowserKeyEventsTest.CommandKeyEvents to fail. See http://crbug.com/49270
  // But using [NSApplication sendEvent:] should be safe for keyboard events,
  // because until now, no code wants to retrieve the next event when handling
  // a keyboard event.
  for (std::vector<NSEvent*>::iterator iter = events.begin();
       iter != events.end(); ++iter)
    [[NSApplication sharedApplication] sendEvent:*iter];

  if (task)
    MessageLoop::current()->PostTask(FROM_HERE, new EventQueueWatcher(task));

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
  CGFloat screenHeight =
    [[[NSScreen screens] objectAtIndex:0] frame].size.height;
  g_mouse_location = NSMakePoint(x, screenHeight - y);  // flip!
  NSPoint pointInWindow = g_mouse_location;
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
    MessageLoop::current()->PostTask(FROM_HERE, new EventQueueWatcher(task));

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
  NSPoint pointInWindow = g_mouse_location;
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
                       clickCount:1
                         pressure:(state == DOWN ? 1.0 : 0.0 )];
  [[NSApplication sharedApplication] postEvent:event atStart:NO];

  if (task)
    MessageLoop::current()->PostTask(FROM_HERE, new EventQueueWatcher(task));

  return true;
}

bool SendMouseClick(MouseButton type) {
 return SendMouseEventsNotifyWhenDone(type, UP|DOWN, NULL);
}

void MoveMouseToCenterAndPress(
    NSView* view,
    MouseButton button,
    int state,
    Task* task) {
  DCHECK(view);
  NSWindow* window = [view window];
  DCHECK(window);
  NSScreen* screen = [window screen];
  DCHECK(screen);

  // Converts the center position of the view into the coordinates accepted
  // by SendMouseMoveNotifyWhenDone() method.
  NSRect bounds = [view bounds];
  NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  center = [view convertPoint:center toView:nil];
  center = [window convertBaseToScreen:center];
  center = NSMakePoint(center.x, [screen frame].size.height - center.y);

  SendMouseMoveNotifyWhenDone(center.x, center.y,
                              new ClickTask(button, state, task));
}

}  // ui_controls
