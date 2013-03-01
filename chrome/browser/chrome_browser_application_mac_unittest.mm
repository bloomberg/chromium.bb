// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsexception_enabler.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

namespace chrome_browser_application_mac {

// Generate an NSException with the given name.
NSException* ExceptionNamed(NSString* name) {
  base::mac::ScopedNSExceptionEnabler enabler;

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
  // TODO(rtenneti): Leaks StatisticsRecorder and will update suppressions.
  base::StatisticsRecorder::Initialize();

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
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histograms[0]->flags());

  scoped_ptr<HistogramSamples> samples(histograms[0]->SnapshotSamples());
  EXPECT_EQ(4, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
  EXPECT_EQ(3, samples->GetCount(2));
  EXPECT_EQ(2, samples->GetCount(3));

  // The unknown exceptions should end up in the overflow bucket.
  EXPECT_TRUE(histograms[0]->HasConstructionArguments(1,
                                                      kUnknownNSException,
                                                      kUnknownNSException + 1));
  EXPECT_EQ(4, samples->GetCount(kUnknownNSException));
}

}  // chrome_browser_application_mac
