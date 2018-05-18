// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_common.h"

#include <stdint.h>

namespace search_provider_logos {

const int64_t kMaxTimeToLiveMS = INT64_C(30 * 24 * 60 * 60 * 1000);  // 30 days

LogoMetadata::LogoMetadata() = default;
LogoMetadata::LogoMetadata(const LogoMetadata&) = default;
LogoMetadata::LogoMetadata(LogoMetadata&&) noexcept = default;
LogoMetadata& LogoMetadata::operator=(const LogoMetadata&) = default;
LogoMetadata& LogoMetadata::operator=(LogoMetadata&&) = default;
LogoMetadata::~LogoMetadata() = default;

EncodedLogo::EncodedLogo() = default;
EncodedLogo::EncodedLogo(const EncodedLogo&) = default;
EncodedLogo::EncodedLogo(EncodedLogo&&) noexcept = default;
EncodedLogo& EncodedLogo::operator=(const EncodedLogo&) = default;
EncodedLogo& EncodedLogo::operator=(EncodedLogo&&) = default;
EncodedLogo::~EncodedLogo() = default;

Logo::Logo() = default;
Logo::~Logo() = default;

LogoCallbacks::LogoCallbacks() = default;
LogoCallbacks::LogoCallbacks(LogoCallbacks&&) = default;
LogoCallbacks& LogoCallbacks::operator=(LogoCallbacks&&) = default;
LogoCallbacks::~LogoCallbacks() = default;

}  // namespace search_provider_logos
