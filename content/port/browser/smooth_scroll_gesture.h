// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_SMOOTH_SCROLL_GESTURE_H_
#define CONTENT_PORT_BROWSER_SMOOTH_SCROLL_GESTURE_H_

#include "base/memory/ref_counted.h"
#include "base/time.h"

namespace content {

class RenderWidgetHost;

// This is a base class representing a single scroll gesture. These gestures are
// paired with the rendering benchmarking system to (automatically) measure how
// smoothnly chrome is responding to user input.
class SmoothScrollGesture : public base::RefCounted<SmoothScrollGesture> {
 public:

  // When called, the gesture should compute its state at the provided timestamp
  // and send the right input events to the provided RenderWidgetHost to
  // simulate the gesture having run up to that point in time.
  //
  // This function should return true until the gesture is over. A false return
  // value will stop ticking the gesture and clean it up.
  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) = 0;
 protected:
  friend class base::RefCounted<SmoothScrollGesture>;
  virtual ~SmoothScrollGesture() {}
};

} // namespace content

#endif // CONTENT_PORT_BROWSER_SMOOTH_SCROLL_GESTURE_H_
