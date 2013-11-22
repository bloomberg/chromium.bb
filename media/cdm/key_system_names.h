// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_KEY_SYSTEM_NAMES_H_
#define MEDIA_CDM_KEY_SYSTEM_NAMES_H_

#include <string>

#include "media/base/media_export.h"

namespace media {

// TODO(jrummell): Change other uses of Clear Key to use this common value.

// The key system names for Clear Key.
MEDIA_EXPORT extern const char kPrefixedClearKey[];
MEDIA_EXPORT extern const char kUnprefixedClearKey[];

// The key system name for External Clear Key.
MEDIA_EXPORT extern const char kExternalClearKey[];

// Returns true if |key_system| is Clear Key, false otherwise.
MEDIA_EXPORT inline bool IsClearKey(const std::string& key_system) {
  return key_system == kPrefixedClearKey || key_system == kUnprefixedClearKey;
}

// Returns true if |key_system| is External Clear Key, false otherwise.
MEDIA_EXPORT bool IsExternalClearKey(const std::string& key_system);

}  // namespace media

#endif  // MEDIA_CDM_KEY_SYSTEM_NAMES_H_
