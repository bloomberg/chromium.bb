// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_UTIL_SWITCHES_H_
#define ATHENA_UTIL_SWITCHES_H_

#include "athena/athena_export.h"

namespace athena {
namespace switches {

ATHENA_EXPORT extern const char kEnableDebugAccelerators[];

bool IsDebugAcceleratorsEnabled();

}  // namespace switches
}  // namespace athena

#endif  // ATHENA_UTIL_SWITCHES_H_
