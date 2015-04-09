// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_HISTOGRAM_MANAGER_H_
#define COMPONENTS_CRONET_HISTOGRAM_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

namespace cronet {

// A HistogramManager instance is created by the app. It is the central
// controller for the acquisition of log data, and recording deltas for
// transmission to an external server.
class HistogramManager : public base::HistogramFlattener {
 public:
  HistogramManager();
  ~HistogramManager() override;

  // Snapshot all histograms to record the delta into |uma_proto_| and then
  // returns the serialized protobuf representation of the record in |data|.
  // Returns true if it was successfully serialized.
  bool GetDeltas(std::vector<uint8>* data);

  // TODO(mef): Hang Histogram Manager off java object instead of singleton.
  static HistogramManager* GetInstance();

 private:
  friend struct base::DefaultLazyInstanceTraits<HistogramManager>;

  // base::HistogramFlattener:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override;
  void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override;
  void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override;
  void InconsistencyDetectedInLoggedCount(int amount) override;

  base::HistogramSnapshotManager histogram_snapshot_manager_;

  // Stores the protocol buffer representation for this log.
  metrics::ChromeUserMetricsExtension uma_proto_;

  DISALLOW_COPY_AND_ASSIGN(HistogramManager);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_HISTOGRAM_MANAGER_H_
