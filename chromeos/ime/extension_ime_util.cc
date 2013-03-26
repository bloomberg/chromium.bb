// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/extension_ime_util.h"

#include "base/string_util.h"

namespace chromeos {
namespace {
const char* kExtensionIMEPrefix = "_ext_ime_";
}  // namespace

namespace extension_ime_util {
std::string GetInputMethodID(const std::string& extension_id,
                             const std::string& engine_id) {
  DCHECK(!extension_id.empty());
  DCHECK(!engine_id.empty());
  return kExtensionIMEPrefix + extension_id + engine_id;
}

bool IsExtensionIME(const std::string& input_method_id) {
  return StartsWithASCII(input_method_id,
                         kExtensionIMEPrefix,
                         true);  // Case sensitive.
}

}  // namespace extension_ime_util
}  // namespace chromeos
