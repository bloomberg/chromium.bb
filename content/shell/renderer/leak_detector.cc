// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/leak_detector.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/shell/renderer/webkit_test_runner.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"

using blink::WebLeakDetector;

namespace content {

// The initial states of the DOM objects at about:blank. The four nodes are a
// Document, a HTML, a HEAD and a BODY.
//
// TODO(hajimehoshi): Now these are hard-corded. If we add target to count like
// RefCoutned objects whose initial state is diffcult to estimate, we stop using
// hard-coded values. Instead, we need to load about:blank ahead of the layout
// tests actually and initialize LeakDetector by the got values.
const int kInitialNumberOfLiveDocuments = 1;
const int kInitialNumberOfLiveNodes = 4;

LeakDetector::LeakDetector(WebKitTestRunner* test_runner)
    : test_runner_(test_runner),
      web_leak_detector_(blink::WebLeakDetector::create(this)) {
  previous_result_.numberOfLiveDocuments = kInitialNumberOfLiveDocuments;
  previous_result_.numberOfLiveNodes = kInitialNumberOfLiveNodes;
}

LeakDetector::~LeakDetector() {
}

void LeakDetector::TryLeakDetection(blink::WebLocalFrame* frame) {
  web_leak_detector_->collectGarbageAndGetDOMCounts(frame);
}

void LeakDetector::onLeakDetectionComplete(
    const WebLeakDetectorClient::Result& result) {
  LeakDetectionResult report;
  report.leaked =
      (previous_result_.numberOfLiveDocuments < result.numberOfLiveDocuments ||
       previous_result_.numberOfLiveNodes < result.numberOfLiveNodes);

  if (report.leaked) {
    base::DictionaryValue detail;
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveDocuments);
    list->AppendInteger(result.numberOfLiveDocuments);
    detail.Set("numberOfLiveDocuments", list);

    list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveNodes);
    list->AppendInteger(result.numberOfLiveNodes);
    detail.Set("numberOfLiveNodes", list);

    std::string detail_str;
    base::JSONWriter::Write(&detail, &detail_str);
    report.detail = detail_str;
  }

  previous_result_ = result;
  test_runner_->ReportLeakDetectionResult(report);
}

}  // namespace content
