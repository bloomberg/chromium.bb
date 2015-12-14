// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BINDER_UTIL_H_
#define BINDER_UTIL_H_

#include "base/basictypes.h"

namespace binder {

// Returns the string representation of the given binder command or "UNKNOWN"
// if command is unknown, never returns null.
const char* CommandToString(uint32 command);

}  // namespace binder

#endif  // BINDER_UTIL_H_
