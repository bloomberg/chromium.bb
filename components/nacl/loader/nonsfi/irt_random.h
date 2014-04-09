// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_IRT_RANDOM_H_
#define COMPONENTS_NACL_LOADER_NONSFI_IRT_RANDOM_H_

namespace nacl {
namespace nonsfi {

// Set FD for urandom for use, needs to be initialized once at
// startup. Relies on the caller to clean up the FD, which is the case
// when using base::GetUrandomFD().
void SetUrandomFd(int fd);

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_IRT_RANDOM_H_
