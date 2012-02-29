// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHADOW_TYPES_H_
#define ASH_WM_SHADOW_TYPES_H_
#pragma once

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

// Different types of drop shadows that can be drawn under a window by the
// shell.  Used as a value for the kShadowTypeKey property.
enum ShadowType {
  // Starts at 0 due to the cast in GetShadowType().
  SHADOW_TYPE_NONE = 0,
  SHADOW_TYPE_RECTANGULAR,
};

ASH_EXPORT void SetShadowType(aura::Window* window, ShadowType shadow_type);
ASH_EXPORT ShadowType GetShadowType(aura::Window* window);

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SHADOW_TYPES_H_
