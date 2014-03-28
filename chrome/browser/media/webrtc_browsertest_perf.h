// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_PERF_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_PERF_H_

namespace base {
class DictionaryValue;
}

namespace test {

// These functions takes parsed data (on one peer connection) from the
// peerConnectionDataStore object that is backing webrtc-internals and prints
// metrics they consider interesting using testing/perf/perf_test.h primitives.
// The idea is to put as many webrtc-related metrics as possible into the
// dashboard and thereby track it for regressions.
//
// These functions expect to run under googletest and will use EXPECT_ and
// ASSERT_ macros to signal failure.
void PrintBweForVideoMetrics(const base::DictionaryValue& pc_dict);
void PrintMetricsForAllStreams(const base::DictionaryValue& pc_dict);

}  // namespace test

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_PERF_H_
