// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"

namespace content {

// Throttles Pepper Plugin instances in Power Saver mode. Currently just a
// stub implementation that engages throttling after a fixed timeout.
// In the future, will examine plugin frames to find a suitable preview
// image before engaging throttling.
class PepperPluginInstanceThrottler {
 public:
  // |throttle_closure| is called to engage throttling.
  explicit PepperPluginInstanceThrottler(const base::Closure& throttle_closure);

  virtual ~PepperPluginInstanceThrottler();

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperPluginInstanceThrottler);
};
}

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_
