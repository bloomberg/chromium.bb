// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/leak_detector.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/shell/renderer/layout_test/blink_test_runner.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"

using blink::WebLeakDetector;

namespace content {

// The initial states of the DOM objects at about:blank. The four nodes are a
// Document, a HTML, a HEAD and a BODY.
//
// TODO(hajimehoshi): Now these are hard-corded. If we add a target to count
// objects like RefCounted whose initial state is diffcult to estimate, we stop
// using hard-coded values. Instead, we need to load about:blank ahead of the
// layout tests actually and initialize LeakDetector by the got values.
const int kInitialNumberOfLiveAudioNodes = 0;
const int kInitialNumberOfLiveDocuments = 1;
const int kInitialNumberOfLiveNodes = 4;
const int kInitialNumberOfLiveLayoutObjects = 3;
const int kInitialNumberOfLiveResources = 0;
const int kInitialNumberOfScriptPromises = 0;
const int kInitialNumberOfLiveFrames = 1;
const int kInitialNumberOfWorkerGlobalScopes = 0;

// In the initial state, there are two SuspendableObjects (FontFaceSet created
// by HTMLDocument and SuspendableTimer created by DocumentLoader).
const int kInitialNumberOfLiveSuspendableObject = 2;

// This includes not only about:blank's context but also ScriptRegexp (e.g.
// created by isValidEmailAddress in EmailInputType.cpp). The leak detector
// always creates the latter to stabilize the number of V8PerContextData
// objects.
const int kInitialNumberOfV8PerContextData = 2;

LeakDetector::LeakDetector(BlinkTestRunner* test_runner)
    : test_runner_(test_runner),
      web_leak_detector_(blink::WebLeakDetector::create(this)) {
  previous_result_.numberOfLiveAudioNodes = kInitialNumberOfLiveAudioNodes;
  previous_result_.numberOfLiveDocuments = kInitialNumberOfLiveDocuments;
  previous_result_.numberOfLiveNodes = kInitialNumberOfLiveNodes;
  previous_result_.numberOfLiveLayoutObjects =
      kInitialNumberOfLiveLayoutObjects;
  previous_result_.numberOfLiveResources = kInitialNumberOfLiveResources;
  previous_result_.numberOfLiveSuspendableObjects =
    kInitialNumberOfLiveSuspendableObject;
  previous_result_.numberOfLiveScriptPromises = kInitialNumberOfScriptPromises;
  previous_result_.numberOfLiveFrames = kInitialNumberOfLiveFrames;
  previous_result_.numberOfLiveV8PerContextData =
    kInitialNumberOfV8PerContextData;
  previous_result_.numberOfWorkerGlobalScopes =
    kInitialNumberOfWorkerGlobalScopes;
}

LeakDetector::~LeakDetector() {
}

void LeakDetector::TryLeakDetection(blink::WebFrame* frame) {
  web_leak_detector_->prepareForLeakDetection(frame);
  web_leak_detector_->collectGarbageAndReport();
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
  if (previous_result_.numberOfLiveLayoutObjects <
      result.numberOfLiveLayoutObjects) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveLayoutObjects);
    list->AppendInteger(result.numberOfLiveLayoutObjects);
    detail.Set("numberOfLiveLayoutObjects", list);
  }
  if (previous_result_.numberOfLiveResources < result.numberOfLiveResources) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveResources);
    list->AppendInteger(result.numberOfLiveResources);
    detail.Set("numberOfLiveResources", list);
  }
  if (previous_result_.numberOfLiveSuspendableObjects <
      result.numberOfLiveSuspendableObjects) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveSuspendableObjects);
    list->AppendInteger(result.numberOfLiveSuspendableObjects);
    detail.Set("numberOfLiveSuspendableObjects", list);
  }
  if (previous_result_.numberOfLiveScriptPromises <
      result.numberOfLiveScriptPromises) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveScriptPromises);
    list->AppendInteger(result.numberOfLiveScriptPromises);
    detail.Set("numberOfLiveScriptPromises", list);
  }
  if (previous_result_.numberOfLiveFrames < result.numberOfLiveFrames) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveFrames);
    list->AppendInteger(result.numberOfLiveFrames);
    detail.Set("numberOfLiveFrames", list);
  }
  if (previous_result_.numberOfLiveV8PerContextData <
      result.numberOfLiveV8PerContextData) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfLiveV8PerContextData);
    list->AppendInteger(result.numberOfLiveV8PerContextData);
    detail.Set("numberOfLiveV8PerContextData", list);
  }
  if (previous_result_.numberOfWorkerGlobalScopes <
      result.numberOfWorkerGlobalScopes) {
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_result_.numberOfWorkerGlobalScopes);
    list->AppendInteger(result.numberOfWorkerGlobalScopes);
    detail.Set("numberOfWorkerGlobalScopes", list);
  }

  if (!detail.empty()) {
    std::string detail_str;
    base::JSONWriter::Write(detail, &detail_str);
    report.detail = detail_str;
    report.leaked = true;
  }

  previous_result_ = result;
  test_runner_->ReportLeakDetectionResult(report);
}

}  // namespace content
