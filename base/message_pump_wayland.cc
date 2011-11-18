// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_wayland.h"

#include <glib.h>

namespace base {

MessagePumpWayland::MessagePumpWayland()
    : MessagePumpGlib(),
      context_(g_main_context_default()) {
}

MessagePumpWayland::~MessagePumpWayland() {
}

bool MessagePumpWayland::RunOnce(GMainContext* context, bool block) {
  // g_main_context_iteration returns true if events have been dispatched.
  return g_main_context_iteration(context, block);
}

}  // namespace base
