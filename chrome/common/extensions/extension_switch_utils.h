// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_SWITCH_UTILS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_SWITCH_UTILS_H_

namespace extensions {

namespace switch_utils {

bool IsEasyOffStoreInstallEnabled();

bool IsActionBoxEnabled();

bool IsExtensionsInActionBoxEnabled();

bool AreScriptBadgesEnabled();

}  // switch_utils

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_SWITCH_UTILS_H_
