// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_data.h"

#include <utility>

namespace ash {

ToastData::ToastData(std::string id,
                     std::string text,
                     int32_t duration_ms,
                     std::string dismiss_text)
    : id(std::move(id)),
      text(std::move(text)),
      duration_ms(duration_ms),
      dismiss_text(std::move(dismiss_text)) {}

ToastData::ToastData(const ToastData& other) = default;

}  // namespace ash
