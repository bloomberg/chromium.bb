// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/third_party_dlls/hook.h"

namespace third_party_dlls {

HookStatus ApplyHook() {
  return HookStatus::kSuccess;
}

}  // namespace third_party_dlls
