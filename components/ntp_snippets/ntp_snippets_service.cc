// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

namespace ntp_snippets {

NTPSnippetsService::NTPSnippetsService(
    const std::string& application_language_code)
    : application_language_code_(application_language_code) {}

NTPSnippetsService::~NTPSnippetsService() {}

void NTPSnippetsService::Shutdown() {}

}  // namespace ntp_snippets
