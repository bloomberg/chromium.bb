// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class MostVisitedIframeSourceTest : public testing::Test {
 public:
  void ExpectNullData(base::RefCountedMemory* data) {
    EXPECT_EQ(NULL, data);
  }

 protected:
  MostVisitedIframeSource* source() { return source_.get(); }

 private:
  virtual void SetUp() { source_.reset(new MostVisitedIframeSource()); }

  scoped_ptr<MostVisitedIframeSource> source_;
};
