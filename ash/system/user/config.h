// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_CONFIG_H_
#define ASH_SYSTEM_USER_CONFIG_H_

#include "base/macros.h"

namespace ash {
namespace tray {

// Returns true when multi profile is supported and user is active.
bool IsMultiProfileSupportedAndUserActive();

// Returns true when multi account is supported and user is active.
bool IsMultiAccountSupportedAndUserActive();

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_CONFIG_H_
