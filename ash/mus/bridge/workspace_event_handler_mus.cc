// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/workspace_event_handler_mus.h"

#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(ash::mus::WorkspaceEventHandlerMus*);

namespace {

MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(ash::mus::WorkspaceEventHandlerMus*,
                                     kWorkspaceEventHandlerProperty,
                                     nullptr);

}  // namespace

namespace ash {
namespace mus {

WorkspaceEventHandlerMus::WorkspaceEventHandlerMus(ui::Window* workspace_window)
    : workspace_window_(workspace_window) {
  workspace_window_->SetLocalProperty(kWorkspaceEventHandlerProperty, this);
}

WorkspaceEventHandlerMus::~WorkspaceEventHandlerMus() {
  workspace_window_->ClearLocalProperty(kWorkspaceEventHandlerProperty);
}

// static
WorkspaceEventHandlerMus* WorkspaceEventHandlerMus::Get(ui::Window* window) {
  return window->GetLocalProperty(kWorkspaceEventHandlerProperty);
}

}  // namespace mus
}  // namespace ash
