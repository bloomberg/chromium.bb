// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_dispatcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

namespace ash {

namespace {

#if defined(USE_OZONE)
bool IsKeyEvent(const base::NativeEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  return event->IsKeyEvent();
}
#elif defined(USE_X11)
bool IsKeyEvent(const XEvent* xev) {
  return xev->type == KeyPress || xev->type == KeyRelease;
}
#else
#error Unknown build platform: you should have either use_ozone or use_x11.
#endif

scoped_ptr<ui::ScopedEventDispatcher> OverrideDispatcher(
    ui::PlatformEventDispatcher* dispatcher) {
  ui::PlatformEventSource* source = ui::PlatformEventSource::GetInstance();
  return source ? source->OverrideDispatcher(dispatcher)
                : scoped_ptr<ui::ScopedEventDispatcher>();
}

}  // namespace

class AcceleratorDispatcherLinux : public AcceleratorDispatcher,
                                   public ui::PlatformEventDispatcher {
 public:
  AcceleratorDispatcherLinux()
      : restore_dispatcher_(OverrideDispatcher(this)) {}

  virtual ~AcceleratorDispatcherLinux() {}

 private:
  // AcceleratorDispatcher:
  virtual scoped_ptr<base::RunLoop> CreateRunLoop() OVERRIDE {
    return scoped_ptr<base::RunLoop>(new base::RunLoop());
  }

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    return true;
  }

  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    if (IsKeyEvent(event)) {
      ui::KeyEvent key_event(event, false);
      if (MenuClosedForPossibleAccelerator(key_event)) {
#if defined(USE_X11)
        XPutBackEvent(event->xany.display, event);
#else
        NOTIMPLEMENTED();
#endif
        return ui::POST_DISPATCH_QUIT_LOOP;
      }

      if (AcceleratorProcessedForKeyEvent(key_event))
        return ui::POST_DISPATCH_NONE;
    }

    ui::PlatformEventDispatcher* prev = *restore_dispatcher_;
    return prev ? prev->DispatchEvent(event)
                : ui::POST_DISPATCH_PERFORM_DEFAULT;
  }

  scoped_ptr<ui::ScopedEventDispatcher> restore_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorDispatcherLinux);
};

scoped_ptr<AcceleratorDispatcher> AcceleratorDispatcher::Create(
    base::MessagePumpDispatcher* nested_dispatcher) {
  return scoped_ptr<AcceleratorDispatcher>(new AcceleratorDispatcherLinux());
}

}  // namespace ash
