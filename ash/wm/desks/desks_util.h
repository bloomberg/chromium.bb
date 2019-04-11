// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_UTIL_H_
#define ASH_WM_DESKS_DESKS_UTIL_H_

#include <array>
#include <vector>

#include "ash/ash_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

namespace desks_util {

// TODO(afakhry): Fix the size of the array when you add the rest of the desks'
// containters.
constexpr size_t kMaxNumberOfDesks = 1;

ASH_EXPORT const std::array<int, kMaxNumberOfDesks>& GetDesksContainersIds();

ASH_EXPORT std::vector<aura::Window*> GetDesksContainers(aura::Window* root);

ASH_EXPORT const char* GetDeskContainerName(int container_id);

ASH_EXPORT bool IsDeskContainer(const aura::Window* container);

ASH_EXPORT bool IsDeskContainerId(int id);

ASH_EXPORT int GetActiveDeskContainerId();

ASH_EXPORT bool IsActiveDeskContainer(const aura::Window* container);

ASH_EXPORT aura::Window* GetActiveDeskContainerForRoot(aura::Window* root);

}  // namespace desks_util

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_UTIL_H_
