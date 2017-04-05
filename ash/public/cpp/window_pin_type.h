// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PIN_TYPE_H_
#define ASH_PUBLIC_CPP_WINDOW_PIN_TYPE_H_

#include <cstdint>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

ASH_PUBLIC_EXPORT bool IsValidWindowPinType(int64_t value);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PIN_TYPE_H_
