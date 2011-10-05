// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
#pragma once

#include <string>

namespace extension_test_util {

// Makes a fake extension id using the given |seed|.
std::string MakeId(std::string seed);

}  // namespace extension_test_util

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
