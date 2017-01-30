// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/workspace_event_handler_mus.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(ash::mus::WorkspaceEventHandlerMus*);

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(ash::mus::WorkspaceEventHandlerMus*,
                             kWorkspaceEventHandlerProperty,
                             nullptr);

}  // namespace

namespace ash {
namespace mus {

WorkspaceEventHandlerMus::WorkspaceEventHandlerMus(
    aura::Window* workspace_window)
    : workspace_window_(workspace_window) {
  workspace_window_->SetProperty(kWorkspaceEventHandlerProperty, this);
}

WorkspaceEventHandlerMus::~WorkspaceEventHandlerMus() {
  workspace_window_->ClearProperty(kWorkspaceEventHandlerProperty);
}

// static
WorkspaceEventHandlerMus* WorkspaceEventHandlerMus::Get(aura::Window* window) {
  return window->GetProperty(kWorkspaceEventHandlerProperty);
}

}  // namespace mus
}  // namespace ash
