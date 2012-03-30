// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_PROTECTOR_UTILS_H_
#define CHROME_BROWSER_PROTECTOR_PROTECTOR_UTILS_H_
#pragma once

#include <string>

namespace protector {

// Signs string value with protector's key.
std::string SignSetting(const std::string& value);

// Returns true if the signature is valid for the specified key.
bool IsSettingValid(const std::string& value, const std::string& signature);

// Whether the Protector feature is enabled.
bool IsEnabled();

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_PROTECTOR_UTILS_H_
