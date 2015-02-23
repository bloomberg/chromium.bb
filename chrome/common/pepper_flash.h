// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PEPPER_FLASH_H_
#define CHROME_COMMON_PEPPER_FLASH_H_

#include "base/basictypes.h"
#include "base/values.h"
#include "base/version.h"

namespace chrome {
// Permission bits for Pepper Flash.
extern const int32 kPepperFlashPermissions;

// Returns true if this browser is compatible with the given Pepper Flash
// manifest, with the version specified in the manifest in |version_out|.
bool CheckPepperFlashManifest(const base::DictionaryValue& manifest,
                              base::Version* version_out);
}  // namespace chrome

#endif  // CHROME_COMMON_PEPPER_FLASH_H_
