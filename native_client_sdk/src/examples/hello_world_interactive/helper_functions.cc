// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "helper_functions.h"

namespace hello_world {

int32_t FortyTwo() { return 42; }

std::string ReverseText(const std::string& text) {
  std::string reversed_string(text);
  // Use reverse to reverse |reversed_string| in place.
  std::reverse(reversed_string.begin(), reversed_string.end());
  return reversed_string;
}
}  // namespace hello_world
