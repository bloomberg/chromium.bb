// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_dispatcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/run_loop.h"
#include "ui/events/event.h"

using base::MessagePumpDispatcher;

namespace ash {

namespace {

bool IsKeyEvent(const MSG& msg) {
  return msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
         msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP;
}

}  // namespace

class AcceleratorDispatcherWin : public AcceleratorDispatcher,
                                 public MessagePumpDispatcher {
 public:
  explicit AcceleratorDispatcherWin(MessagePumpDispatcher* nested)
      : nested_dispatcher_(nested) {}
  virtual ~AcceleratorDispatcherWin() {}

 private:
  // AcceleratorDispatcher:
  virtual scoped_ptr<base::RunLoop> CreateRunLoop() OVERRIDE {
    return scoped_ptr<base::RunLoop>(new base::RunLoop(this));
  }

  // MessagePumpDispatcher:
  virtual uint32_t Dispatch(const MSG& event) OVERRIDE {
    if (IsKeyEvent(event)) {
      ui::KeyEvent key_event(event, false);
      if (MenuClosedForPossibleAccelerator(key_event))
        return POST_DISPATCH_QUIT_LOOP;

      if (AcceleratorProcessedForKeyEvent(key_event))
        return POST_DISPATCH_NONE;
    }

    return nested_dispatcher_ ? nested_dispatcher_->Dispatch(event)
                              : POST_DISPATCH_PERFORM_DEFAULT;
  }

  MessagePumpDispatcher* nested_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorDispatcherWin);
};

scoped_ptr<AcceleratorDispatcher> AcceleratorDispatcher::Create(
    MessagePumpDispatcher* nested_dispatcher) {
  return scoped_ptr<AcceleratorDispatcher>(
      new AcceleratorDispatcherWin(nested_dispatcher));
}

}  // namespace ash
