// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/base/test_windows.h"

#include "athena/screen/public/screen_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace athena {
namespace test {

scoped_ptr<aura::Window> CreateNormalWindow(aura::WindowDelegate* delegate,
                                            aura::Window* parent) {
  return CreateWindowWithType(delegate, parent, ui::wm::WINDOW_TYPE_NORMAL);
}

scoped_ptr<aura::Window> CreateWindowWithType(aura::WindowDelegate* delegate,
                                              aura::Window* parent,
                                              ui::wm::WindowType window_type) {
  scoped_ptr<aura::Window> window(new aura::Window(delegate));
  window->SetType(window_type);
  window->Init(aura::WINDOW_LAYER_SOLID_COLOR);
  if (parent) {
    parent->AddChild(window.get());
  } else {
    aura::client::ParentWindowWithContext(
        window.get(), ScreenManager::Get()->GetContext(), gfx::Rect());
  }
  return window.Pass();
}

scoped_ptr<aura::Window> CreateTransientWindow(aura::WindowDelegate* delegate,
                                               aura::Window* transient_parent,
                                               ui::ModalType modal_type,
                                               bool top_most) {
  scoped_ptr<aura::Window> window(new aura::Window(delegate));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_SOLID_COLOR);
  window->SetProperty(aura::client::kModalKey, modal_type);
  window->SetProperty(aura::client::kAlwaysOnTopKey, top_most);
  if (transient_parent)
    wm::AddTransientChild(transient_parent, window.get());
  aura::client::ParentWindowWithContext(
      window.get(), ScreenManager::Get()->GetContext(), gfx::Rect());
  return window.Pass();
}

}  // namespace test
}  // namespace athena
