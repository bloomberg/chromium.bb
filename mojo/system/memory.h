// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MEMORY_H_
#define MOJO_SYSTEM_MEMORY_H_

#include <stddef.h>

namespace mojo {
namespace system {

// Verify that |count * size_each| bytes can be read from the user |pointer|
// insofar as possible/necessary. |count| and |size_each| are specified
// separately instead of a single size, since |count * size_each| may overflow a
// |size_t|. |count| may be zero but |size_each| must be nonzero.
//
// For example, if running in kernel mode, this should be a full verification
// that the given memory is owned and readable by the user process. In user
// mode, if crashes are acceptable, this may do nothing at all (and always
// return true).
bool VerifyUserPointer(const void* pointer, size_t count, size_t size_each);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MEMORY_H_
