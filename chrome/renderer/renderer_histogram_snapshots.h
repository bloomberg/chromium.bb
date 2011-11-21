// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_HISTOGRAM_SNAPSHOTS_H_
#define CHROME_RENDERER_RENDERER_HISTOGRAM_SNAPSHOTS_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/task.h"
#include "chrome/common/metrics_helpers.h"
#include "content/public/renderer/render_process_observer.h"

class RendererHistogramSnapshots : public HistogramSender,
                                   public content::RenderProcessObserver {
 public:
  RendererHistogramSnapshots();
  virtual ~RendererHistogramSnapshots();

  // Send the histogram data.
  void SendHistograms(int sequence_number);

 private:
  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnGetRendererHistograms(int sequence_number);

  // Maintain a map of histogram names to the sample stats we've sent.
  typedef std::map<std::string, base::Histogram::SampleSet> LoggedSampleMap;
  typedef std::vector<std::string> HistogramPickledList;

  // Extract snapshot data and then send it off the the Browser process.
  // Send only a delta to what we have already sent.
  void UploadAllHistrograms(int sequence_number);

  base::WeakPtrFactory<RendererHistogramSnapshots> weak_factory_;

  // HistogramSender interface (override) methods.
  virtual void TransmitHistogramDelta(
      const base::Histogram& histogram,
      const base::Histogram::SampleSet& snapshot) OVERRIDE;
  virtual void InconsistencyDetected(int problem) OVERRIDE;
  virtual void UniqueInconsistencyDetected(int problem) OVERRIDE;
  virtual void SnapshotProblemResolved(int amount) OVERRIDE;

  // Collection of histograms to send to the browser.
  HistogramPickledList pickled_histograms_;

  DISALLOW_COPY_AND_ASSIGN(RendererHistogramSnapshots);
};

#endif  // CHROME_RENDERER_RENDERER_HISTOGRAM_SNAPSHOTS_H_
