// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_entry_builder.h"

#include "base/metrics/metrics_hashes.h"
#include "components/ukm/ukm_entry.h"
#include "components/ukm/ukm_service.h"

namespace ukm {

UkmEntryBuilder::UkmEntryBuilder(const UkmService::AddEntryCallback& callback,
                                 int32_t source_id,
                                 const char* event_name)
    : add_entry_callback_(callback),
      entry_(new UkmEntry(source_id, event_name)) {}

UkmEntryBuilder::~UkmEntryBuilder() {
  if (entry_->metrics_.empty())
    return;

  add_entry_callback_.Run(std::move(entry_));
}

void UkmEntryBuilder::AddMetric(const char* metric_name, int64_t value) {
  entry_->metrics_.emplace_back(
      std::make_pair(base::HashMetricName(metric_name), value));
}

}  // namespace ukm
