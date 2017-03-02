// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_ENTRY_H_
#define COMPONENTS_UKM_UKM_ENTRY_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace ukm {

class Entry;
class UkmEntryBuilder;

// One UkmEntry contains metrics for a specific source and event. It is
// connected to a UkmSource by the source ID. The event can be user defined.
// One example is "PageLoad". Each UkmEntry can have a list of metrics, each of
// which consist of a metric name and value. When the entry is serialized to the
// proto message, the event and metric names will be hashed by
// base::HashMetricName.
//
// To build UkmEntry objects, please use UkmEntryBuilder.
class UkmEntry {
 public:
  // Serializes the members of the class into the supplied proto.
  void PopulateProto(Entry* proto_entry) const;

  int32_t source_id() const { return source_id_; }

  ~UkmEntry();

 private:
  friend UkmEntryBuilder;

  UkmEntry(int32_t source_id, const char* event_name);

  const int32_t source_id_;
  const uint64_t event_hash_;
  std::vector<std::pair<uint64_t, int64_t>> metrics_;

  DISALLOW_COPY_AND_ASSIGN(UkmEntry);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_ENTRY_H_
