// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/api_feature.h"

namespace extensions {

APIFeature::APIFeature()
    : internal_(false), blocked_in_service_worker_(false) {}

APIFeature::~APIFeature() {
}

bool APIFeature::IsInternal() const {
  return internal_;
}

bool APIFeature::IsBlockedInServiceWorker() const {
  return blocked_in_service_worker_;
}

std::string APIFeature::Parse(const base::DictionaryValue* value) {
  std::string error = SimpleFeature::Parse(value);
  if (!error.empty())
    return error;

  value->GetBoolean("internal", &internal_);
  value->GetBoolean("blocked_in_service_worker", &blocked_in_service_worker_);

  if (contexts()->empty())
    return name() + ": API features must specify at least one context.";

  return std::string();
}

}  // namespace extensions
