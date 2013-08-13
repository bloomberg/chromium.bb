// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_main_platform_delegate.h"

#import <Cocoa/Cocoa.h>
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/nacl/common/nacl_sandbox_type_mac.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/sandbox_init.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters) {
}

NaClMainPlatformDelegate::~NaClMainPlatformDelegate() {
}

void NaClMainPlatformDelegate::EnableSandbox() {
  CHECK(content::InitializeSandbox(NACL_SANDBOX_TYPE_NACL_LOADER,
                                   base::FilePath()))
      << "Error initializing sandbox for " << switches::kNaClLoaderProcess;
}
