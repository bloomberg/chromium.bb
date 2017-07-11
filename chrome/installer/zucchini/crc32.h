// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_CRC32_H_
#define CHROME_INSTALLER_ZUCCHINI_CRC32_H_

#include <cstdint>

namespace zucchini {

// Calculates CRC-32 of the given range [|first|, |last|).
uint32_t CalculateCrc32(const uint8_t* first, const uint8_t* last);

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_CRC32_H_
