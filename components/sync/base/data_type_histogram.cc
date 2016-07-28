// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type_histogram.h"

#include "base/metrics/sparse_histogram.h"

void SyncRecordDatatypeBin(const std::string& name, int sample, int value) {
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      name, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(sample, value);
}
