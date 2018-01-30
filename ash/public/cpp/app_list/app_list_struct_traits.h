// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_STRUCT_TRAITS_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_STRUCT_TRAITS_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/interfaces/app_list.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<ash::mojom::AppListState, ash::AppListState> {
  static ash::mojom::AppListState ToMojom(ash::AppListState input) {
    switch (input) {
      case ash::AppListState::kStateApps:
        return ash::mojom::AppListState::kStateApps;
      case ash::AppListState::kStateSearchResults:
        return ash::mojom::AppListState::kStateSearchResults;
      case ash::AppListState::kStateStart:
        return ash::mojom::AppListState::kStateStart;
      case ash::AppListState::kStateCustomLauncherPageDeprecated:
      case ash::AppListState::kInvalidState:
        break;
    }
    NOTREACHED();
    return ash::mojom::AppListState::kStateApps;
  }

  static bool FromMojom(ash::mojom::AppListState input,
                        ash::AppListState* out) {
    switch (input) {
      case ash::mojom::AppListState::kStateApps:
        *out = ash::AppListState::kStateApps;
        return true;
      case ash::mojom::AppListState::kStateSearchResults:
        *out = ash::AppListState::kStateSearchResults;
        return true;
      case ash::mojom::AppListState::kStateStart:
        *out = ash::AppListState::kStateStart;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ash::mojom::AppListModelStatus, ash::AppListModelStatus> {
  static ash::mojom::AppListModelStatus ToMojom(ash::AppListModelStatus input) {
    switch (input) {
      case ash::AppListModelStatus::kStatusNormal:
        return ash::mojom::AppListModelStatus::kStatusNormal;
      case ash::AppListModelStatus::kStatusSyncing:
        return ash::mojom::AppListModelStatus::kStatusSyncing;
    }
    NOTREACHED();
    return ash::mojom::AppListModelStatus::kStatusNormal;
  }

  static bool FromMojom(ash::mojom::AppListModelStatus input,
                        ash::AppListModelStatus* out) {
    switch (input) {
      case ash::mojom::AppListModelStatus::kStatusNormal:
        *out = ash::AppListModelStatus::kStatusNormal;
        return true;
      case ash::mojom::AppListModelStatus::kStatusSyncing:
        *out = ash::AppListModelStatus::kStatusSyncing;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_STRUCT_TRAITS_H_
