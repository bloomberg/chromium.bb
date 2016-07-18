// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/api_feature.h"

namespace extensions {

APIFeature::APIFeature() {}

APIFeature::~APIFeature() {
}

bool APIFeature::Validate(std::string* error) {
  if (!SimpleFeature::Validate(error))
    return false;

  if (contexts()->empty()) {
    *error = name() + ": API features must specify at least one context.";
    return false;
  }

  return true;
}

}  // namespace extensions
