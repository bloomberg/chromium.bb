// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_LEAK_DETECTOR_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_LEAK_DETECTOR_H_

#include <memory>

#include "base/macros.h"
#include "content/shell/common/leak_detection_result.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"

namespace blink {
class WebFrame;
}  // namespace blink

namespace content {

class BlinkTestRunner;

// LeakDetector counts DOM objects and compare them between two pages.
class LeakDetector : public blink::WebLeakDetectorClient {
 public:
  LeakDetector(BlinkTestRunner* test_runner);
  virtual ~LeakDetector();

  // Counts DOM objects, compare the previous status and returns the result of
  // leak detection. It is assumed that this method is always called when a
  // specific page, like about:blank is loaded to compare the previous
  // circumstance of DOM objects. If the number of objects increses, there
  // should be a leak.
  void TryLeakDetection(blink::WebFrame*);

  // WebLeakDetectorClient:
  void OnLeakDetectionComplete(const Result& result) override;

 private:
  BlinkTestRunner* test_runner_;
  blink::WebLeakDetectorClient::Result previous_result_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetector);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_LEAK_DETECTOR_H_
