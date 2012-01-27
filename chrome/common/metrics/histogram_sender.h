// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_HISTOGRAM_SENDER_H_
#define CHROME_COMMON_METRICS_HISTOGRAM_SENDER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"

// HistogramSender handles the logistics of gathering up available histograms
// for transmission (such as from renderer to browser, or from browser to UMA
// upload).  It has several pure virtual functions that are replaced in
// derived classes to allow the exact lower level transmission mechanism,
// or error report mechanism, to be replaced.  Since histograms can sit in
// memory for an extended period of time, and are vulnerable to memory
// corruption, this class also validates as much rendundancy as it can before
// calling for the marginal change (a.k.a., delta) in a histogram to be sent
// onward.
class HistogramSender {
 protected:
  HistogramSender();
  virtual ~HistogramSender();

  // Snapshot all histograms, and transmit the delta.
  // The arguments allow a derived class to select only a subset for
  // transmission, or to set a flag in each transmitted histogram.
  void TransmitAllHistograms(base::Histogram::Flags flags_to_set,
                             bool send_only_uma);

  // Send the histograms onward, as defined in a derived class.
  // This is only called with a delta, listing samples that have not previously
  // been transmitted.
  virtual void TransmitHistogramDelta(
      const base::Histogram& histogram,
      const base::Histogram::SampleSet& snapshot) = 0;

  // Record various errors found during attempts to send histograms.
  virtual void InconsistencyDetected(int problem) = 0;
  virtual void UniqueInconsistencyDetected(int problem) = 0;
  virtual void SnapshotProblemResolved(int amount) = 0;

 private:
  // Maintain a map of histogram names to the sample stats we've sent.
  typedef std::map<std::string, base::Histogram::SampleSet> LoggedSampleMap;
  // List of histograms names, and their encontered corruptions.
  typedef std::map<std::string, int> ProblemMap;

  // Snapshot this histogram, and transmit the delta.
  void TransmitHistogram(const base::Histogram& histogram);

  // For histograms, record what we've already transmitted (as a sample for each
  // histogram) so that we can send only the delta with the next log.
  LoggedSampleMap logged_samples_;

  // List of histograms found corrupt to be corrupt, and their problems.
  scoped_ptr<ProblemMap> inconsistencies_;

  DISALLOW_COPY_AND_ASSIGN(HistogramSender);
};

#endif  // CHROME_COMMON_METRICS_HISTOGRAM_SENDER_H_
