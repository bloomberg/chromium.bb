// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_entry.h"

#include "base/metrics/metrics_hashes.h"
#include "components/metrics/proto/ukm/entry.pb.h"
#include "components/ukm/ukm_source.h"

namespace ukm {

UkmEntry::UkmEntry(int32_t source_id, const char* event_name)
    : source_id_(source_id), event_hash_(base::HashMetricName(event_name)) {}

UkmEntry::~UkmEntry() {}

void UkmEntry::PopulateProto(Entry* proto_entry) const {
  DCHECK(!proto_entry->has_source_id());
  DCHECK(!proto_entry->has_event_hash());
  DCHECK(proto_entry->metrics_size() == 0);

  proto_entry->set_source_id(source_id_);
  proto_entry->set_event_hash(event_hash_);
  for (auto& metric : metrics_) {
    Entry::Metric* proto_metric = proto_entry->add_metrics();
    proto_metric->set_metric_hash(std::get<0>(metric));
    proto_metric->set_value(std::get<1>(metric));
  }
}

}  // namespace ukm
