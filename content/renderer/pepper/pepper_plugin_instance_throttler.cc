// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_plugin_instance_throttler.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace content {

namespace {

// When we give up waiting for a suitable preview frame, and simply suspend
// the plugin where it's at. In milliseconds.
const int kThrottleTimeout = 5000;
}

PepperPluginInstanceThrottler::PepperPluginInstanceThrottler(
    const base::Closure& throttle_closure) {
  DCHECK(!throttle_closure.is_null());
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, throttle_closure,
      base::TimeDelta::FromMilliseconds(kThrottleTimeout));
}

PepperPluginInstanceThrottler::~PepperPluginInstanceThrottler() {
}

}  // namespace content
