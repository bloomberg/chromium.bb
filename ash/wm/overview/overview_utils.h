// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_

#include "ash/ash_export.h"

namespace ash {

// Returns true if overview mode should use the new ui.
// TODO(sammiequon): Remove this function when the old overview mode ui becomes
// obsolete. See https://crbug.com/782320.
bool IsNewOverviewUi();

// Resets the stored value so that the next IsNewOverviewUi call will query the
// command line arguments again.
ASH_EXPORT void ResetCachedOverviewUiValueForTesting();

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
