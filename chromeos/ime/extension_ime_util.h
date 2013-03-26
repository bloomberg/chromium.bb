// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_EXTENSION_IME_UTIL_H_
#define CHROMEOS_IME_EXTENSION_IME_UTIL_H_

#include <string>

#include "chromeos/chromeos_export.h"

namespace chromeos {

// Extension IME related utilities.
namespace extension_ime_util {

// Returns InputMethodID for |engine_id| in |extension_id|.
std::string CHROMEOS_EXPORT GetInputMethodID(const std::string& extension_id,
                                             const std::string& engine_id);

// Returns true if the |input_method_id| is extension ime.
bool CHROMEOS_EXPORT IsExtensionIME(const std::string& input_method_id);

}  // extension_ime_util

}  // namespace chromeos

#endif  // CHROMEOS_IME_EXTENSION_IME_UTIL_H_
