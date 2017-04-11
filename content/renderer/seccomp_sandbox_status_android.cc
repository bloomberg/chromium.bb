// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/seccomp_sandbox_status_android.h"

namespace content {

static SeccompSandboxStatus g_status = SeccompSandboxStatus::NOT_SUPPORTED;

void SetSeccompSandboxStatus(SeccompSandboxStatus status) {
  g_status = status;
}

SeccompSandboxStatus GetSeccompSandboxStatus() {
  return g_status;
}

}  // namespace content
