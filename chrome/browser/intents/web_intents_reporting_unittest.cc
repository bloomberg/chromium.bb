// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_reporting.h"

#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_intents {
namespace reporting {
namespace {

const char kCustomAction[] = "jiggle";
const char kImageType[] = "image/png";
const char kCustomType[] = "http://a.b/mine";

UMABucket ToBucket(const std::string& action, const std::string& type) {
  return web_intents::ToUMABucket(
      ASCIIToUTF16(action), ASCIIToUTF16(type));
}

}  // namespace

TEST(WebIntentsReportingTest, RecognizedActionAndType) {
  EXPECT_EQ(BUCKET_SHARE_IMAGE, ToBucket(kActionShare, kImageType));
}

TEST(WebIntentsReportingTest, CustomAction) {
  EXPECT_EQ(BUCKET_CUSTOM_IMAGE, ToBucket(kCustomAction, kImageType));
}

TEST(WebIntentsReportingTest, CustomType) {
  EXPECT_EQ(BUCKET_EDIT_CUSTOM, ToBucket(kActionEdit, kCustomType));
}

TEST(WebIntentsReportingTest, CustomActionAndType) {
  EXPECT_EQ(BUCKET_CUSTOM_CUSTOM, ToBucket(kCustomAction, kCustomType));
}

TEST(WebIntentsUtilTest, MaxValueConstMatchesLastKnownMaxValue) {
  EXPECT_EQ(kMaxActionTypeHistogramValue, BUCKET_VIEW_VIDEO);
}

}  // namespace reporting
}  // namespace web_intents
