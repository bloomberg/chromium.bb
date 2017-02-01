// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/mus_property_mirror_ash.h"

#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace {

template <typename T>
void MirrorOwnedProperty(aura::Window* window,
                         aura::Window* root_window,
                         const aura::WindowProperty<T*>* key) {
  T* value = window->GetProperty(key);
  if (!value)
    root_window->ClearProperty(key);
  else
    root_window->SetProperty(key, new T(*value));
}

}  // namespace

namespace ash {

MusPropertyMirrorAsh::MusPropertyMirrorAsh() {}
MusPropertyMirrorAsh::~MusPropertyMirrorAsh() {}

void MusPropertyMirrorAsh::MirrorPropertyFromWidgetWindowToRootWindow(
    aura::Window* window,
    aura::Window* root_window,
    const void* key) {
  if (key == kPanelAttachedKey) {
    bool value = window->GetProperty(kPanelAttachedKey);
    root_window->SetProperty(kPanelAttachedKey, value);
  } else if (key == kShelfItemTypeKey) {
    int32_t value = window->GetProperty(kShelfItemTypeKey);
    root_window->SetProperty(kShelfItemTypeKey, value);
  } else if (key == aura::client::kAppIconKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kAppIconKey);
  } else if (key == aura::client::kAppIdKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kAppIdKey);
  } else if (key == aura::client::kDrawAttentionKey) {
    bool value = window->GetProperty(aura::client::kDrawAttentionKey);
    root_window->SetProperty(aura::client::kDrawAttentionKey, value);
  } else if (key == aura::client::kTitleKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kTitleKey);
  } else if (key == aura::client::kWindowIconKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kWindowIconKey);
  }
}

}  // namespace ash
