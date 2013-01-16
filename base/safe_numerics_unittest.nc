// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/safe_numerics.h"

namespace base {
namespace internal {

void NoFloatingPoint {
  IsValidNumericCast<float>(0.0);
  IsValidNumericCast<double>(0.0f);
  IsValidNumericCast<int>(DBL_MAX);
}

}  // namespace internal
}  // namespace base
