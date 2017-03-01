// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_ENTRY_BUILDER_H
#define COMPONENTS_UKM_UKM_ENTRY_BUILDER_H

#include <string>

#include "base/macros.h"
#include "components/ukm/ukm_service.h"

namespace ukm {

class UkmEntry;
class UkmService;

// The builder that builds UkmEntry and adds it to UkmService.
// The example usage is:
//
// {
//   unique_ptr<UkmEntryBuilder> builder =
//       ukm_service->GetEntryBuilder(source_id, "PageLoad");
//   builder->AddMetric("NavigationStart", navigation_start_time);
//   builder->AddMetric("ResponseStart", response_start_time);
//   builder->AddMetric("FirstPaint", first_paint_time);
//   builder->AddMetric("FirstContentfulPaint", fcp_time);
// }
//
// When there exists an added metric, the builder will automatically add the
// UkmEntry to UkmService upon destruction when going out of scope.
class UkmEntryBuilder {
 public:
  // Add metric to the entry. A metric contains a metric name and value.
  void AddMetric(const char* metric_name, int64_t value);

  ~UkmEntryBuilder();

 private:
  friend class UkmService;

  UkmEntryBuilder(const UkmService::AddEntryCallback& callback,
                  int32_t source_id,
                  const char* event_name);

  UkmService::AddEntryCallback add_entry_callback_;
  std::unique_ptr<UkmEntry> entry_;

  DISALLOW_COPY_AND_ASSIGN(UkmEntryBuilder);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_ENTRY_BUILDER_H
