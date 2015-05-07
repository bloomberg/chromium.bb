// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/test/web_test_suite.h"

#include "base/metrics/statistics_recorder.h"

namespace web {

WebTestSuite::WebTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

WebTestSuite::~WebTestSuite() {
}

void WebTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Initialize the histograms subsystem, so that any histograms hit in tests
  // are correctly registered with the statistics recorder and can be queried
  // by tests.
  base::StatisticsRecorder::Initialize();
}

}  // namespace web
