// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/leak_detector.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"

using blink::WebLeakDetector;

namespace content {

LeakDetector::LeakDetector()
    : previous_number_of_live_documents_(0),
      previous_number_of_live_nodes_(0) {
}

LeakDetectionResult LeakDetector::TryLeakDetection(blink::WebFrame* frame) {
  LeakDetectionResult result;
  result.number_of_live_documents = 0;
  result.number_of_live_nodes = 0;

  WebLeakDetector::collectGarbargeAndGetDOMCounts(
      frame, &result.number_of_live_documents, &result.number_of_live_nodes);

  result.leaked =
      previous_number_of_live_documents_ > 0 &&
      previous_number_of_live_nodes_ > 0 &&
      (previous_number_of_live_documents_ < result.number_of_live_documents ||
       previous_number_of_live_nodes_ < result.number_of_live_nodes);

  previous_number_of_live_documents_ = result.number_of_live_documents;
  previous_number_of_live_nodes_ = result.number_of_live_nodes;

  return result;
}

}  // namespace content
