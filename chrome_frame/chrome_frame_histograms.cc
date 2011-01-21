// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_histograms.h"

#include "base/metrics/histogram.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"

using base::Histogram;
using base::StatisticsRecorder;

  // Initialize histogram statistics gathering system.
base::LazyInstance<StatisticsRecorder>
    g_statistics_recorder_(base::LINKER_INITIALIZED);

ChromeFrameHistogramSnapshots::ChromeFrameHistogramSnapshots() {
  // Ensure that an instance of the StatisticsRecorder object is created.
  g_statistics_recorder_.Get();
}

ChromeFrameHistogramSnapshots::HistogramPickledList
  ChromeFrameHistogramSnapshots::GatherAllHistograms() {

  base::AutoLock auto_lock(lock_);

  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);

  HistogramPickledList pickled_histograms;

  for (StatisticsRecorder::Histograms::iterator it = histograms.begin();
       histograms.end() != it;
       it++) {
    (*it)->SetFlags(Histogram::kIPCSerializationSourceFlag);
    GatherHistogram(**it, &pickled_histograms);
  }

  return pickled_histograms;
}

void ChromeFrameHistogramSnapshots::GatherHistogram(
    const Histogram& histogram,
    HistogramPickledList* pickled_histograms) {

  // Get up-to-date snapshot of sample stats.
  Histogram::SampleSet snapshot;
  histogram.SnapshotSample(&snapshot);
  const std::string& histogram_name = histogram.histogram_name();

  // Check if we already have a log of this histogram and if not create an
  // empty set.
  LoggedSampleMap::iterator it = logged_samples_.find(histogram_name);
  Histogram::SampleSet* already_logged;
  if (logged_samples_.end() == it) {
    // Add new entry.
    already_logged = &logged_samples_[histogram.histogram_name()];
    already_logged->Resize(histogram);  // Complete initialization.
  } else {
    already_logged = &(it->second);
    // Deduct any stats we've already logged from our snapshot.
    snapshot.Subtract(*already_logged);
  }

  // Snapshot now contains only a delta to what we've already_logged.
  if (snapshot.TotalCount() > 0) {
    GatherHistogramDelta(histogram, snapshot, pickled_histograms);
    // Add new data into our running total.
    already_logged->Add(snapshot);
  }
}

void ChromeFrameHistogramSnapshots::GatherHistogramDelta(
    const Histogram& histogram,
    const Histogram::SampleSet& snapshot,
    HistogramPickledList* pickled_histograms) {

  DCHECK(0 != snapshot.TotalCount());
  snapshot.CheckSize(histogram);

  std::string histogram_info =
      Histogram::SerializeHistogramInfo(histogram, snapshot);
  pickled_histograms->push_back(histogram_info);
}
