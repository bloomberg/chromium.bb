// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_main_platform_delegate.h"

#include "base/logging.h"
#include "sandbox/win/src/sandbox.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters) {
}

NaClMainPlatformDelegate::~NaClMainPlatformDelegate() {
}

void NaClMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  CHECK(target_services) << "NaCl-Win EnableSandbox: No Target Services!";
  // Cause advapi32 to load before the sandbox is turned on.
  unsigned int dummy_rand;
  rand_s(&dummy_rand);
  // Warm up language subsystems before the sandbox is turned on.
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();
  // Turn the sandbox on.
  target_services->LowerToken();
}
