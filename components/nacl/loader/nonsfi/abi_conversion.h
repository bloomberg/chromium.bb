// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_ABI_CONVERSION_H_
#define COMPONENTS_NACL_LOADER_NONSFI_ABI_CONVERSION_H_

struct stat;
struct nacl_abi_stat;

namespace nacl {
namespace nonsfi {

// Converts the stat struct from host's to NaCl's ABI.
void StatToNaClAbiStat(
    const struct stat& host_stat, struct nacl_abi_stat* nacl_stat);

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_ABI_CONVERSION_H_
