// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_NONSFI_MAIN_H_
#define COMPONENTS_NACL_LOADER_NONSFI_NONSFI_MAIN_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/public/imc_types.h"

namespace nacl {
namespace nonsfi {

// Launch NaCl with Non SFI mode.
void MainStart(NaClHandle imc_bootstrap_handle);

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_NONSFI_MAIN_H_
