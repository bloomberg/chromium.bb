// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_IME_SANDBOX_HOOK_H_
#define CHROMEOS_SERVICES_IME_IME_SANDBOX_HOOK_H_

#include "services/service_manager/sandbox/linux/sandbox_linux.h"

namespace chromeos {
namespace ime {

bool ImePreSandboxHook(service_manager::SandboxLinux::Options options);

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_IME_SANDBOX_HOOK_H_
