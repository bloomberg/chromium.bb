// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_

#include <stdint.h>
#include <string>
#include <unordered_map>

#include "base/optional.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/proto/hints.pb.h"

// A representation of optimization guide information related to a navigation.
class OptimizationGuideNavigationData {
 public:
  explicit OptimizationGuideNavigationData(int64_t navigation_id);
  ~OptimizationGuideNavigationData();

  OptimizationGuideNavigationData(const OptimizationGuideNavigationData& other);

  // Records metrics based on data currently held in |this|.
  void RecordMetrics() const;

  // The navigation ID of the navigation handle that this data is associated
  // with.
  int64_t navigation_id() const { return navigation_id_; }

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string() const {
    return serialized_hint_version_string_;
  }
  void set_serialized_hint_version_string(
      const std::string& serialized_hint_version_string) {
    serialized_hint_version_string_ = serialized_hint_version_string;
  }

  // Returns the latest decision made for |optimization_type|.
  base::Optional<optimization_guide::OptimizationTypeDecision>
  GetDecisionForOptimizationType(
      optimization_guide::proto::OptimizationType optimization_type) const;
  // Sets the |decision| for |optimization_type|.
  void SetDecisionForOptimizationType(
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationTypeDecision decision);

  // Returns the latest decision made for |optimmization_target|.
  base::Optional<optimization_guide::OptimizationTargetDecision>
  GetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget optimization_target) const;
  // Sets the |decision| for |optimization_target|.
  void SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetDecision decision);

 private:
  // The navigation ID of the navigation handle that this data is associated
  // with.
  const int64_t navigation_id_;

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string_ = base::nullopt;

  // The map from optimization type to the last decision made for that type.
  std::unordered_map<optimization_guide::proto::OptimizationType,
                     optimization_guide::OptimizationTypeDecision>
      optimization_type_decisions_;

  // The map from optimization target to the last decision made for that target.
  std::unordered_map<optimization_guide::OptimizationTarget,
                     optimization_guide::OptimizationTargetDecision>
      optimization_target_decisions_;

  DISALLOW_ASSIGN(OptimizationGuideNavigationData);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
