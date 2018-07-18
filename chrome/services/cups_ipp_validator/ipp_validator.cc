// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_validator/ipp_validator.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chrome {

IppValidator::IppValidator(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

IppValidator::~IppValidator() = default;

}  // namespace chrome
