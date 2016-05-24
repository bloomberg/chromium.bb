// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_DATA_H_
#define ASH_SYSTEM_TOAST_TOAST_DATA_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

struct ASH_EXPORT ToastData {
  ToastData(const std::string& id,
            const std::string& text,
            uint64_t duration_ms)
      : id(id), text(text), duration_ms(duration_ms) {}

  std::string id;
  std::string text;
  uint64_t duration_ms;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_DATA_H_
