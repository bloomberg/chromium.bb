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
const int kInitialNumberOfLiveAudioNodes = 0;
const int kInitialNumberOfLiveDocuments = 1;
const int kInitialNumberOfLiveNodes = 4;
const int kInitialNumberOfLiveRenderObjects = 3;
const int kInitialNumberOfLiveResources = 1;

LeakDetector::LeakDetector(WebKitTestRunner* test_runner)
    : test_runner_(test_runner),
      web_leak_detector_(blink::WebLeakDetector::create(this)) {
  previous_result_.numberOfLiveAudioNodes = kInitialNumberOfLiveAudioNodes;
  previous_result_.numberOfLiveDocuments = kInitialNumberOfLiveDocuments;
  previous_result_.numberOfLiveNodes = kInitialNumberOfLiveNodes;
  previous_result_.numberOfLiveRenderObjects =
      kInitialNumberOfLiveRenderObjects;
  previous_result_.numberOfLiveResources = kInitialNumberOfLiveResources;
}

LeakDetector::~LeakDetector() {
}

void LeakDetector::TryLeakDetection(blink::WebLocalFrame* frame) {
  web_leak_detector_->collectGarbageAndGetDOMCounts(frame);
}

void LeakDetector::onLeakDetectionComplete(
    const WebLeakDetectorClient::Result& result) {
  LeakDetectionResult report;
  report.leaked = false;
  base::DictionaryValue detail;

  if (previous_result_.numberOfLiveAudioNodes < result.numberOfLiveAudioNodes) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveAudioNodes);
    list->AppendInteger(result.numberOfLiveAudioNodes);
    detail.Set("numberOfLiveAudioNodes", list);
  }
  if (previous_result_.numberOfLiveDocuments < result.numberOfLiveDocuments) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveDocuments);
    list->AppendInteger(result.numberOfLiveDocuments);
    detail.Set("numberOfLiveDocuments", list);
  }
  if (previous_result_.numberOfLiveNodes < result.numberOfLiveNodes) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveNodes);
    list->AppendInteger(result.numberOfLiveNodes);
    detail.Set("numberOfLiveNodes", list);
  }
  if (previous_result_.numberOfLiveRenderObjects <
      result.numberOfLiveRenderObjects) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveRenderObjects);
    list->AppendInteger(result.numberOfLiveRenderObjects);
    detail.Set("numberOfLiveRenderObjects", list);
  }
  if (previous_result_.numberOfLiveResources < result.numberOfLiveResources) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveResources);
    list->AppendInteger(result.numberOfLiveResources);
    detail.Set("numberOfLiveResources", list);
  }

  if (!detail.empty()) {
    std::string detail_str;
    base::JSONWriter::Write(&detail, &detail_str);
    report.detail = detail_str;
    report.leaked = true;
  }

  previous_result_ = result;
  test_runner_->ReportLeakDetectionResult(report);
}

}  // namespace content
