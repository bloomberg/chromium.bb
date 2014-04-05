// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_OBSERVER_H
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_OBSERVER_H

#include "base/base_export.h"
#include "base/event_types.h"

namespace base {

// A MessagePumpObserver is an object that receives global
// notifications from the UI MessageLoop with MessagePumpWin or
// MessagePumpX11.
//
// NOTE: An Observer implementation should be extremely fast!
//
// For use with MessagePumpX11, please see message_pump_glib.h for more
// info about how this is invoked in this environment.
class BASE_EXPORT MessagePumpObserver {
 public:
  // This method is called before processing a NativeEvent.
  virtual void WillProcessEvent(const NativeEvent& event) = 0;

  // This method is called after processing a message.
  virtual void DidProcessEvent(const NativeEvent& event) = 0;

 protected:
  virtual ~MessagePumpObserver() {}
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_OBSERVER_H
