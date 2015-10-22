// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_dimmer.h"

#include "ash/shell.h"
#include "ash/wm/dim_window.h"
#include "base/time/time.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::ScreenDimmer*);

namespace ash {
namespace {
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ScreenDimmer, kScreenDimmerKey, nullptr);

// Opacity when it's dimming the entire screen.
const float kDimmingLayerOpacityForRoot = 0.4f;

const int kRootWindowMagicId = -100;

std::vector<aura::Window*> GetAllContainers(int container_id) {
  return container_id == kRootWindowMagicId
             ? Shell::GetAllRootWindows()
             : Shell::GetContainersFromAllRootWindows(container_id, nullptr);
}

}  // namespace

// static
ScreenDimmer* ScreenDimmer::GetForContainer(int container_id) {
  aura::Window* primary_container = FindContainer(container_id);
  ScreenDimmer* dimmer = primary_container->GetProperty(kScreenDimmerKey);
  if (!dimmer) {
    dimmer = new ScreenDimmer(container_id);
    primary_container->SetProperty(kScreenDimmerKey, dimmer);
  }
  return dimmer;
}

// static
ScreenDimmer* ScreenDimmer::GetForRoot() {
  ScreenDimmer* dimmer = GetForContainer(kRootWindowMagicId);
  // Root window's dimmer
  dimmer->target_opacity_ = kDimmingLayerOpacityForRoot;
  return dimmer;
}

ScreenDimmer::ScreenDimmer(int container_id)
    : container_id_(container_id), target_opacity_(0.5f), is_dimming_(false) {
  Shell::GetInstance()->AddShellObserver(this);
}

ScreenDimmer::~ScreenDimmer() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

void ScreenDimmer::SetDimming(bool should_dim) {
  if (should_dim == is_dimming_)
    return;
  is_dimming_ = should_dim;

  Update(should_dim);
}

ScreenDimmer* ScreenDimmer::FindForTest(int container_id) {
  return FindContainer(container_id)->GetProperty(kScreenDimmerKey);
}

// static
aura::Window* ScreenDimmer::FindContainer(int container_id) {
  aura::Window* primary = Shell::GetPrimaryRootWindow();
  return container_id == kRootWindowMagicId
             ? primary
             : primary->GetChildById(container_id);
}

void ScreenDimmer::OnRootWindowAdded(aura::Window* root_window) {
  Update(is_dimming_);
}

void ScreenDimmer::Update(bool should_dim) {
  for (aura::Window* container : GetAllContainers(container_id_)) {
    DimWindow* dim = DimWindow::Get(container);
    if (should_dim) {
      if (!dim) {
        dim = new DimWindow(container);
        dim->SetDimOpacity(target_opacity_);
      }
      dim->Show();
    } else {
      if (dim) {
        dim->Hide();
        delete dim;
      }
    }
  }
}

}  // namespace ash
