// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_IMAGE_UTIL_H_
#define EXTENSIONS_BROWSER_IMAGE_UTIL_H_

#include <string>

typedef unsigned int SkColor;

// This file contains various utility functions for extension images and colors.
namespace extensions {
namespace image_util {

// Parses a string like #FF9982 or #EEE to a color. Returns true for success.
bool ParseCSSColorString(const std::string& color_string, SkColor* result);

}  // namespace image_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_IMAGE_UTIL_H__
