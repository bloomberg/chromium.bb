// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LEAK_DETECTOR_H_
#define CONTENT_SHELL_RENDERER_LEAK_DETECTOR_H_

#include "base/basictypes.h"
#include "content/shell/common/leak_detection_result.h"
// TODO(dcheng): Temporary. Convert back to a forward declare.
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

// LeakDetector counts DOM objects and compare them between two pages.
class LeakDetector {
 public:
  LeakDetector();

  // Counts DOM objects, compare the previous status and returns the result of
  // leak detection. It is assumed that this method is always called when a
  // specific page, like about:blank is loaded to compare the previous
  // circumstance of DOM objects. If the number of objects increses, there
  // should be a leak.
  LeakDetectionResult TryLeakDetection(blink::WebLocalFrame* frame);

 private:
  // The number of the live documents last time.
  unsigned previous_number_of_live_documents_;

  // The number of the live nodes last time.
  unsigned previous_number_of_live_nodes_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetector);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LEAK_DETECTOR_H_
