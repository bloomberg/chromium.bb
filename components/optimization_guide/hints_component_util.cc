// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_component_util.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

namespace {

void RecordProcessHintsComponentResult(ProcessHintsComponentResult result) {
  UMA_HISTOGRAM_ENUMERATION(kProcessHintsComponentResultHistogramString,
                            static_cast<int>(result),
                            static_cast<int>(ProcessHintsComponentResult::MAX));
}

}  // namespace

const char kProcessHintsComponentResultHistogramString[] =
    "OptimizationGuide.ProcessHintsResult";

std::unique_ptr<proto::Configuration> ProcessHintsComponent(
    const HintsComponentInfo& component_info) {
  if (!component_info.version.IsValid() || component_info.path.empty()) {
    RecordProcessHintsComponentResult(
        ProcessHintsComponentResult::FAILED_INVALID_PARAMETERS);
    return nullptr;
  }

  std::string binary_pb;
  if (!base::ReadFileToString(component_info.path, &binary_pb)) {
    RecordProcessHintsComponentResult(
        ProcessHintsComponentResult::FAILED_READING_FILE);
    return nullptr;
  }

  std::unique_ptr<proto::Configuration> proto_configuration =
      std::make_unique<proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    RecordProcessHintsComponentResult(
        ProcessHintsComponentResult::FAILED_INVALID_CONFIGURATION);
    return nullptr;
  }

  RecordProcessHintsComponentResult(ProcessHintsComponentResult::SUCCESS);
  return proto_configuration;
}

}  // namespace optimization_guide
