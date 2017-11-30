// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This file is only compiled when Crashpad is used as the crash
// reproter.

#include "components/crash/core/common/crash_key.h"

#include "components/crash/core/common/crash_key_base_support.h"
#include "third_party/crashpad/crashpad/client/annotation_list.h"

namespace crash_reporter {

void InitializeCrashKeys() {
  crashpad::AnnotationList::Register();
  InitializeCrashKeyBaseSupport();
}

}  // namespace crash_reporter
