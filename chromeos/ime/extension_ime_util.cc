// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/extension_ime_util.h"

#include "base/strings/string_util.h"

namespace chromeos {
namespace {
const char kExtensionIMEPrefix[] = "_ext_ime_";
const int kExtensionIMEPrefixLength =
    sizeof(kExtensionIMEPrefix) / sizeof(kExtensionIMEPrefix[0]) - 1;
const char kComponentExtensionIMEPrefix[] = "_comp_ime_";
const char kExtensionXkbIdPrefix[] =
    "_comp_ime_fgoepimhcoialccpbmpnnblemnepkkao";
const int kComponentExtensionIMEPrefixLength =
    sizeof(kComponentExtensionIMEPrefix) /
        sizeof(kComponentExtensionIMEPrefix[0]) - 1;
const int kExtensionIdLength = 32;
// Hard coded to true. If the wrapped extension keyboards misbehaves,
// we can easily change this to false to switch back to legacy xkb keyboards.
bool g_use_wrapped_extension_keyboard_layouts = true;
}  // namespace

namespace extension_ime_util {
std::string GetInputMethodID(const std::string& extension_id,
                             const std::string& engine_id) {
  DCHECK(!extension_id.empty());
  DCHECK(!engine_id.empty());
  return kExtensionIMEPrefix + extension_id + engine_id;
}

std::string GetComponentInputMethodID(const std::string& extension_id,
                                      const std::string& engine_id) {
  DCHECK(!extension_id.empty());
  DCHECK(!engine_id.empty());
  return kComponentExtensionIMEPrefix + extension_id + engine_id;
}

std::string GetExtensionIDFromInputMethodID(
    const std::string& input_method_id) {
  if (IsExtensionIME(input_method_id) &&
      input_method_id.size() >= kExtensionIMEPrefixLength +
                                kExtensionIdLength) {
    return input_method_id.substr(kExtensionIMEPrefixLength,
                                  kExtensionIdLength);
  }
  if (IsComponentExtensionIME(input_method_id) &&
      input_method_id.size() >= kComponentExtensionIMEPrefixLength +
                                kExtensionIdLength) {
    return input_method_id.substr(kComponentExtensionIMEPrefixLength,
                                  kExtensionIdLength);
  }
  return "";
}

std::string GetInputMethodIDByKeyboardLayout(
    const std::string& keyboard_layout_id) {
  bool migrate = UseWrappedExtensionKeyboardLayouts();
  if (IsKeyboardLayoutExtension(keyboard_layout_id)) {
    if (migrate)
      return keyboard_layout_id;
    return keyboard_layout_id.substr(arraysize(kExtensionXkbIdPrefix) - 1);
  }
  if (migrate && StartsWithASCII(keyboard_layout_id, "xkb:", true))
    return kExtensionXkbIdPrefix + keyboard_layout_id;
  return keyboard_layout_id;
}

bool IsExtensionIME(const std::string& input_method_id) {
  return StartsWithASCII(input_method_id,
                         kExtensionIMEPrefix,
                         true);  // Case sensitive.
}

bool IsComponentExtensionIME(const std::string& input_method_id) {
  return StartsWithASCII(input_method_id,
                         kComponentExtensionIMEPrefix,
                         true);  // Case sensitive.
}

bool IsMemberOfExtension(const std::string& input_method_id,
                         const std::string& extension_id) {
  return StartsWithASCII(input_method_id,
                         kExtensionIMEPrefix + extension_id,
                         true);  // Case sensitive.
}

bool IsKeyboardLayoutExtension(const std::string& input_method_id) {
  return StartsWithASCII(input_method_id, kExtensionXkbIdPrefix, true);
}

bool UseWrappedExtensionKeyboardLayouts() {
  return g_use_wrapped_extension_keyboard_layouts;
}

std::string MaybeGetLegacyXkbId(const std::string& input_method_id) {
  if (IsKeyboardLayoutExtension(input_method_id)) {
    size_t pos = input_method_id.find("xkb:");
    if (pos != std::string::npos)
      return input_method_id.substr(pos);
  }
  return input_method_id;
}

ScopedUseExtensionKeyboardFlagForTesting::
    ScopedUseExtensionKeyboardFlagForTesting(bool new_flag)
  : auto_reset_(&g_use_wrapped_extension_keyboard_layouts, new_flag) {
}

ScopedUseExtensionKeyboardFlagForTesting::
    ~ScopedUseExtensionKeyboardFlagForTesting() {
}

}  // namespace extension_ime_util
}  // namespace chromeos
