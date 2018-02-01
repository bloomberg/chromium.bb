// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_UPDATER_UMA_H_
#define EXTENSIONS_COMMON_EXTENSION_UPDATER_UMA_H_

namespace extensions {

// These enum values are used in UMA, they should NOT be reordered.
enum class ExtensionUpdaterUpdateResult {
  NO_UPDATE = 0,
  UPDATE_SUCCESS = 1,
  UPDATE_ERROR = 2,

  UPDATE_RESULT_COUNT
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_UPDATER_UMA_H_
