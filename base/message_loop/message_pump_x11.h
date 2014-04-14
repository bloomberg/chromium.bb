// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_X11_H
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_X11_H

#include <bitset>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_glib.h"
#include "base/observer_list.h"

typedef struct _XDisplay Display;

namespace base {

// This class implements a message-pump for dispatching X events.
class BASE_EXPORT MessagePumpX11 : public MessagePumpGlib {
 public:
  MessagePumpX11();
  virtual ~MessagePumpX11();

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

  // Returns the UI or GPU message pump.
  static MessagePumpX11* Current();

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagePumpX11);
};

#if !defined(TOOLKIT_GTK)
typedef MessagePumpX11 MessagePumpForUI;
#endif

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_X11_H
