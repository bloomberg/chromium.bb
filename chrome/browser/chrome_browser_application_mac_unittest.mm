// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/metrics/histogram.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Histogram;
using base::StatisticsRecorder;

namespace chrome_browser_application_mac {

// Generate an NSException with the given name.
NSException* ExceptionNamed(NSString* name) {
  return [NSException exceptionWithName:name
                                 reason:@"No reason given"
                               userInfo:nil];
}

// Helper to keep binning expectations readible.
size_t BinForExceptionNamed(NSString* name) {
  return BinForException(ExceptionNamed(name));
}

TEST(ChromeApplicationMacTest, ExceptionBinning) {
  // These exceptions must be in this order.
  EXPECT_EQ(BinForExceptionNamed(NSGenericException), 0U);
  EXPECT_EQ(BinForExceptionNamed(NSRangeException), 1U);
  EXPECT_EQ(BinForExceptionNamed(NSInvalidArgumentException), 2U);
  EXPECT_EQ(BinForExceptionNamed(NSMallocException), 3U);

  // Random other exceptions map to |kUnknownNSException|.
  EXPECT_EQ(BinForExceptionNamed(@"CustomName"), kUnknownNSException);
  EXPECT_EQ(BinForExceptionNamed(@"Custom Name"), kUnknownNSException);
  EXPECT_EQ(BinForExceptionNamed(@""), kUnknownNSException);
  EXPECT_EQ(BinForException(nil), kUnknownNSException);
}

TEST(ChromeApplicationMacTest, RecordException) {
  // Start up a histogram recorder.
  StatisticsRecorder recorder;

  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetSnapshot("OSX.NSException", &histograms);
  EXPECT_EQ(0U, histograms.size());

  // Record some known exceptions.
  RecordExceptionWithUma(ExceptionNamed(NSGenericException));
  RecordExceptionWithUma(ExceptionNamed(NSGenericException));
  RecordExceptionWithUma(ExceptionNamed(NSGenericException));
  RecordExceptionWithUma(ExceptionNamed(NSGenericException));
  RecordExceptionWithUma(ExceptionNamed(NSRangeException));
  RecordExceptionWithUma(ExceptionNamed(NSInvalidArgumentException));
  RecordExceptionWithUma(ExceptionNamed(NSInvalidArgumentException));
  RecordExceptionWithUma(ExceptionNamed(NSInvalidArgumentException));
  RecordExceptionWithUma(ExceptionNamed(NSMallocException));
  RecordExceptionWithUma(ExceptionNamed(NSMallocException));

  // Record some unknown exceptions.
  RecordExceptionWithUma(ExceptionNamed(@"CustomName"));
  RecordExceptionWithUma(ExceptionNamed(@"Custom Name"));
  RecordExceptionWithUma(ExceptionNamed(@""));
  RecordExceptionWithUma(nil);

  // We should have exactly the right number of exceptions.
  StatisticsRecorder::GetSnapshot("OSX.NSException", &histograms);
  EXPECT_EQ(1U, histograms.size());
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histograms[0]->flags());
  Histogram::SampleSet sample;
  histograms[0]->SnapshotSample(&sample);
  EXPECT_EQ(4, sample.counts(0));
  EXPECT_EQ(1, sample.counts(1));
  EXPECT_EQ(3, sample.counts(2));
  EXPECT_EQ(2, sample.counts(3));

  // The unknown exceptions should end up in the overflow bucket.
  EXPECT_EQ(kUnknownNSException + 1, histograms[0]->bucket_count());
  EXPECT_EQ(4, sample.counts(kUnknownNSException));
}

}  // chrome_browser_application_mac
