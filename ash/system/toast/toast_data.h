// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_DATA_H_
#define ASH_SYSTEM_TOAST_TOAST_DATA_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

struct ASH_EXPORT ToastData {
  // "|duration_ms| == -1" means the toast view should be displayed until the
  // dismiss button is clicked.
  static const int32_t kInfiniteDuration = -1;

  ToastData(std::string id,
            std::string text,
            int32_t duration_ms,
            std::string dismiss_text);
  ToastData(const ToastData& other);

  std::string id;
  std::string text;
  int32_t duration_ms;
  std::string dismiss_text;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_DATA_H_
