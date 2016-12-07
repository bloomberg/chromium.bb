// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_PROPERTIES_H_
#define ASH_MUS_WINDOW_PROPERTIES_H_

#include "ui/aura/window.h"

namespace ash {
namespace mus {

// Maps to ui::mojom::WindowManager::kRenderParentTitleArea_Property.
extern const aura::WindowProperty<bool>* const kRenderTitleAreaProperty;

// Set to true if the window server tells us the window is janky (see
// WindowManagerDelegate::OnWmClientJankinessChanged()).
extern const aura::WindowProperty<bool>* const kWindowIsJanky;

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_PROPERTIES_H_
