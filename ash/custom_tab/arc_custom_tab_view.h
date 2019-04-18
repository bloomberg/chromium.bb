// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
#define ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_

#include <memory>

#include "ash/public/interfaces/arc_custom_tab.mojom.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ws/remote_view_host/server_remote_view_host.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

namespace exo {
class Surface;
}

namespace ash {

// Implementation of ArcCustomTabView interface.
class ArcCustomTabView : public views::View,
                         public mojom::ArcCustomTabView,
                         public aura::WindowObserver {
 public:
  // Creates a new ArcCustomTabView instance. The instance will be deleted when
  // the pointer is closed. Returns null when the arguments are invalid.
  static mojom::ArcCustomTabViewPtr Create(int32_t task_id,
                                           int32_t surface_id,
                                           int32_t top_margin);

  // mojom::ArcCustomTabView:
  void EmbedUsingToken(const base::UnguessableToken& token) override;

  // views::View:
  void Layout() override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  ArcCustomTabView(aura::Window* arc_app_window,
                   int32_t surface_id,
                   int32_t top_margin);
  ~ArcCustomTabView() override;

  // Binds this instance to the pointer.
  void Bind(mojom::ArcCustomTabViewPtr* ptr);

  // Deletes this object when the mojo connection is closed.
  void Close();

  // Converts the point from the given window to this view.
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  // Tries to find the surface.
  exo::Surface* FindSurface();

  mojo::Binding<mojom::ArcCustomTabView> binding_;
  ws::ServerRemoteViewHost* const remote_view_host_;
  aura::Window* const arc_app_window_;
  const int32_t surface_id_, top_margin_;
  base::flat_set<aura::Window*> observed_surfaces_;
  base::WeakPtrFactory<ArcCustomTabView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomTabView);
};

}  // namespace ash

#endif  // ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
