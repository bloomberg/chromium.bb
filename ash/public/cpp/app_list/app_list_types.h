// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// All possible states of the app list.
// Note: Do not change the order of these as they are used for metrics.
enum class ASH_PUBLIC_EXPORT AppListState {
  kStateApps = 0,
  kStateSearchResults,
  kStateStart,
  kStateCustomLauncherPageDeprecated,  // Don't use over IPC
  // Add new values here.

  kInvalidState,               // Don't use over IPC
  kStateLast = kInvalidState,  // Don't use over IPC
};

// The status of the app list model.
enum class ASH_PUBLIC_EXPORT AppListModelStatus {
  kStatusNormal,
  kStatusSyncing,  // Syncing apps or installing synced apps.
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
