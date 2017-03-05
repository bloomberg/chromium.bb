// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/disconnected_app_handler.h"

#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(ash::mus::DisconnectedAppHandler*);

namespace ash {
namespace mus {
namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(DisconnectedAppHandler,
                                   kDisconnectedAppHandlerKey,
                                   nullptr);

}  // namespace

// static
void DisconnectedAppHandler::Create(aura::Window* window) {
  window->SetProperty(kDisconnectedAppHandlerKey,
                      new DisconnectedAppHandler(window));
}

DisconnectedAppHandler::DisconnectedAppHandler(aura::Window* window)
    : window_(window) {
  window->AddObserver(this);
}

DisconnectedAppHandler::~DisconnectedAppHandler() {
  window_->RemoveObserver(this);
}

void DisconnectedAppHandler::OnEmbeddedAppDisconnected(aura::Window* window) {
  delete window_;
}

}  // namespace mus
}  // namespace ash
