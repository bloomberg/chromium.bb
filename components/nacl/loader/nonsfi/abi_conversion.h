// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_ABI_CONVERSION_H_
#define COMPONENTS_NACL_LOADER_NONSFI_ABI_CONVERSION_H_

struct stat;
struct timespec;
struct nacl_abi_stat;
struct nacl_abi_timespec;

namespace nacl {
namespace nonsfi {

// Converts the timespec struct from NaCl's to host's ABI.
void NaClAbiTimeSpecToTimeSpec(const struct nacl_abi_timespec& nacl_timespec,
                               struct timespec* host_timespec);

// Converts the timespec struct from host's to NaCl's ABI.
void TimeSpecToNaClAbiTimeSpec(const struct timespec& host_timespec,
                               struct nacl_abi_timespec* nacl_timespec);


// Converts the stat struct from host's to NaCl's ABI.
void StatToNaClAbiStat(
    const struct stat& host_stat, struct nacl_abi_stat* nacl_stat);

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_ABI_CONVERSION_H_
