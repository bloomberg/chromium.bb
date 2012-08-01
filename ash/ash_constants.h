// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_CONSTANTS_H_
#define ASH_ASH_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {

// The window is a constrained window and lives therefore entirely within
// another aura window.
ASH_EXPORT extern const aura::WindowProperty<bool>* const
    kConstrainedWindowKey;

} // namespace ash

#endif  // ASH_ASH_CONSTANTS_H_
