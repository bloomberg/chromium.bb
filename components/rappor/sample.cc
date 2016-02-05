// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/sample.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "components/rappor/bloom_filter.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/reports.h"

namespace rappor {

Sample::Sample(int32_t cohort_seed, const RapporParameters& parameters)
  : parameters_(parameters),
    bloom_offset_((cohort_seed % parameters_.num_cohorts) *
          parameters_.bloom_filter_hash_function_count) {
  // Must use bloom filter size that fits in uint64_t.
  DCHECK_LE(parameters_.bloom_filter_size_bytes, 8);
}

Sample::~Sample() {
}

void Sample::SetStringField(const std::string& field_name,
                            const std::string& value) {
  DVLOG(2) << "Recording sample \"" << value
           << "\" for sample metric field \"" << field_name << "\"";
  DCHECK_EQ(0u, sizes_[field_name]);
  fields_[field_name] = internal::GetBloomBits(
      parameters_.bloom_filter_size_bytes,
      parameters_.bloom_filter_hash_function_count,
      bloom_offset_,
      value);
  sizes_[field_name] = parameters_.bloom_filter_size_bytes;
}

void Sample::SetFlagsField(const std::string& field_name,
                           uint64_t flags,
                           size_t num_flags) {
  DVLOG(2) << "Recording flags " << flags
           << " for sample metric field \"" << field_name << "\"";
  DCHECK_EQ(0u, sizes_[field_name]);
  DCHECK_GT(num_flags, 0u);
  DCHECK_LE(num_flags, 64u);
  DCHECK(num_flags == 64u || flags >> num_flags == 0);
  fields_[field_name] = flags;
  sizes_[field_name] = (num_flags + 7) / 8;
}

void Sample::ExportMetrics(const std::string& secret,
                           const std::string& metric_name,
                           RapporReports* reports) const {
  for (const auto& kv : fields_) {
    uint64_t value = kv.second;
    const auto it = sizes_.find(kv.first);
    DCHECK(it != sizes_.end());
    size_t size = it->second;
    ByteVector value_bytes(size);
    Uint64ToByteVector(value, size, &value_bytes);
    ByteVector report_bytes = internal::GenerateReport(
        secret,
        internal::kNoiseParametersForLevel[parameters_.noise_level],
        value_bytes);

    RapporReports::Report* report = reports->add_report();
    report->set_name_hash(base::HashMetricName(
        metric_name + "." + kv.first));
    report->set_bits(std::string(report_bytes.begin(), report_bytes.end()));
    DVLOG(2) << "Exporting sample " << metric_name << "." << kv.first;
  }
}

}  // namespace rappor
