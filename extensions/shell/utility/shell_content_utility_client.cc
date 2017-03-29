// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/utility/shell_content_utility_client.h"

namespace extensions {

ShellContentUtilityClient::ShellContentUtilityClient() = default;

ShellContentUtilityClient::~ShellContentUtilityClient() = default;

void ShellContentUtilityClient::UtilityThreadStarted() {
  utility_handler::UtilityThreadStarted();
}

}  // namespace extensions
