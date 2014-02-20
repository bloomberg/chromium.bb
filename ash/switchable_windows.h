// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SWITCHABLE_WINDOWS_H_
#define ASH_SWITCHABLE_WINDOWS_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// List of containers which contain windows that can be switched via Alt+Tab to.
ASH_EXPORT extern const int kSwitchableWindowContainerIds[];

// The number of elements in kSwitchableWindowContainerIds.
ASH_EXPORT extern const size_t kSwitchableWindowContainerIdsLength;

}  // namespace ash

#endif  // ASH_SWITCHABLE_WINDOWS_H_
