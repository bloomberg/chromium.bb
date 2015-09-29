// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"

namespace metrics {

class MetricsService;

// This class limits and documents access to metrics service helper methods.
// These methods are protected so each user has to inherit own program-specific
// specialization and enable access there by declaring friends.
class MetricsServiceAccessor {
 protected:
  // Constructor declared as protected to enable inheritance. Descendants should
  // disallow instantiation.
  MetricsServiceAccessor() {}

  // Registers a field trial name and group with |metrics_service| (if not
  // null), to be used to annotate a UMA report with a particular configuration
  // state. A UMA report will be annotated with this trial group if and only if
  // all events in the report were created after the trial is registered. Only
  // one group name may be registered at a time for a given trial name. Only the
  // last group name that is registered for a given trial name will be recorded.
  // The values passed in must not correspond to any real field trial in the
  // code. Returns true on success.
  // See the comment on MetricsService::RegisterSyntheticFieldTrial for details.
  static bool RegisterSyntheticFieldTrial(MetricsService* metrics_service,
                                          const std::string& trial_name,
                                          const std::string& group_name);

  // Same as RegisterSyntheticFieldTrial above, but takes in the trial name as a
  // hash rather than computing the hash from the string.
  static bool RegisterSyntheticFieldTrialWithNameHash(
      MetricsService* metrics_service,
      uint32_t trial_name_hash,
      const std::string& group_name);

  // Same as RegisterSyntheticFieldTrial above, but takes in the trial and group
  // names as hashes rather than computing those hashes from the strings.
  static bool RegisterSyntheticFieldTrialWithNameAndGroupHash(
      MetricsService* metrics_service,
      uint32_t trial_name_hash,
      uint32_t group_name_hash);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceAccessor);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
