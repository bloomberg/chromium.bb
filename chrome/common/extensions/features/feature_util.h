// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_UTIL_H_

namespace extensions {
namespace feature_util {

// Returns true if service workers are enabled for extension schemes.
bool ExtensionServiceWorkersEnabled();

}  // namespace feature_util
}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_UTIL_H_
