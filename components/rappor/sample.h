// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_SAMPLE_H_
#define COMPONENTS_RAPPOR_SAMPLE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "components/rappor/rappor_parameters.h"

namespace rappor {

class RapporReports;
class RapporService;
class TestSamplerFactory;

// Sample is a container for information about a single instance of some event
// we are sending Rappor data about.  It may contain multiple different fields,
// which describe different details of the event, and they will be sent in the
// same Rappor report, enabling analysis of correlations between those fields.
class Sample {
 public:
  virtual ~Sample();

  // Sets a string value field in this sample.
  virtual void SetStringField(const std::string& field_name,
                              const std::string& value);

  // Sets a group of boolean flags as a field in this sample.
  // |flags| should be a set of boolean flags stored in the lowest |num_flags|
  // bits of |flags|.
  virtual void SetFlagsField(const std::string& field_name,
                             uint64_t flags,
                             size_t num_flags);

  // Generate randomized reports and store them in |reports|.
  virtual void ExportMetrics(const std::string& secret,
                             const std::string& metric_name,
                             RapporReports* reports) const;

  const RapporParameters& parameters() { return parameters_; }

 private:
  friend class TestSamplerFactory;
  friend class RapporService;
  friend class TestSample;

  // Constructs a sample.  Instead of calling this directly, call
  // RapporService::MakeSampleObj to create a sample.
  Sample(int32_t cohort_seed, const RapporParameters& parameters);

  const RapporParameters parameters_;

  // Offset used for bloom filter hash functions.
  uint32_t bloom_offset_;

  // Size of each of the different fields, in bytes.
  std::map<std::string, size_t> sizes_;

  // The non-randomized report values for each field.
  std::map<std::string, uint64_t> fields_;

  DISALLOW_COPY_AND_ASSIGN(Sample);
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_SAMPLE_H_
