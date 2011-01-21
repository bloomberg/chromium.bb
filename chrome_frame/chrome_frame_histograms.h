// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_HISTOGRAM_SNAPSHOTS_H_
#define CHROME_FRAME_HISTOGRAM_SNAPSHOTS_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task.h"

// This class gathers histogram data in the host browser process and
// serializes the data into a vector of strings to be uploaded to the
// Chrome browser process. It records the histogram data which has been
// logged and only uploads the delta with the next log.
// TODO(iyengar)
// This class does not contain any ChromeFrame specific stuff. It should
// be moved to base.
class ChromeFrameHistogramSnapshots {
 public:
  // Maintain a map of histogram names to the sample stats we've sent.
  typedef std::map<std::string, base::Histogram::SampleSet> LoggedSampleMap;
  typedef std::vector<std::string> HistogramPickledList;

  ChromeFrameHistogramSnapshots();
  ~ChromeFrameHistogramSnapshots() {}

  // Extract snapshot data and return it to be sent off to the Chrome browser
  // process.
  // Return only a delta to what we have already sent.
  HistogramPickledList GatherAllHistograms();

 private:
  void GatherHistogram(const base::Histogram& histogram,
                       HistogramPickledList* histograms);

  void GatherHistogramDelta(const base::Histogram& histogram,
                            const base::Histogram::SampleSet& snapshot,
                            HistogramPickledList* histograms);

  // For histograms, record what we've already logged (as a sample for each
  // histogram) so that we can send only the delta with the next log.
  LoggedSampleMap logged_samples_;

  // Synchronizes the histogram gathering operation.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFrameHistogramSnapshots);
};

#endif  // CHROME_RENDERER_HISTOGRAM_SNAPSHOTS_H_
