// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "components/ukm/ukm_source.h"

namespace ukm {

TestUkmRecorder::TestUkmRecorder() {
  EnableRecording();
}

TestUkmRecorder::~TestUkmRecorder() = default;

const UkmSource* TestUkmRecorder::GetSourceForUrl(const char* url) const {
  const UkmSource* source = nullptr;
  for (const auto& kv : sources()) {
    if (kv.second->url() == url) {
      DCHECK_EQ(nullptr, source);
      source = kv.second.get();
    }
  }
  return source;
}

const UkmSource* TestUkmRecorder::GetSourceForSourceId(
    ukm::SourceId source_id) const {
  const UkmSource* source = nullptr;
  for (const auto& kv : sources()) {
    if (kv.second->id() == source_id) {
      DCHECK_EQ(nullptr, source);
      source = kv.second.get();
    }
  }
  return source;
}

const mojom::UkmEntry* TestUkmRecorder::GetEntry(size_t entry_num) const {
  DCHECK_LT(entry_num, entries().size());
  return entries()[entry_num].get();
}

const mojom::UkmEntry* TestUkmRecorder::GetEntryForEntryName(
    const char* entry_name) const {
  for (const auto& it : entries()) {
    if (it->event_hash == base::HashMetricName(entry_name))
      return it.get();
  }
  return nullptr;
}

// static
const mojom::UkmMetric* TestUkmRecorder::FindMetric(
    const mojom::UkmEntry* entry,
    const char* metric_name) {
  for (const auto& metric : entry->metrics) {
    if (metric->metric_hash == base::HashMetricName(metric_name))
      return metric.get();
  }
  return nullptr;
}

}  // namespace ukm
