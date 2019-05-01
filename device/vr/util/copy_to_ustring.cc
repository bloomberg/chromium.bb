// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/copy_to_ustring.h"

#include <algorithm>

#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

void CopyToUString(const base::string16& src, UChar* dest, size_t dest_length) {
  static_assert(sizeof(base::string16::value_type) == sizeof(UChar),
                "Mismatched string16/UChar size.");

  const size_t copy_char_count = std::min(src.size(), dest_length - 1);
  src.copy(dest, copy_char_count);
  std::fill(dest + copy_char_count, dest + dest_length, 0);
}

}  // namespace device
