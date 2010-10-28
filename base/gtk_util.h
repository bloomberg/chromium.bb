// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GTK_UTIL_H_
#define BASE_GTK_UTIL_H_
#pragma once

#include <string>
#include <stdint.h>

namespace gtk_util {

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label);

// Removes the "&" accelerators from a Windows label.
std::string RemoveWindowsStyleAccelerators(const std::string& label);

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0, it's
// assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

}  // namespace gtk_util

#endif  // BASE_GTK_UTIL_H_
