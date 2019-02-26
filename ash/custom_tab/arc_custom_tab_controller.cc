// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/custom_tab/arc_custom_tab_controller.h"

#include <utility>

#include "ash/custom_tab/arc_custom_tab_view.h"

namespace ash {

ArcCustomTabController::ArcCustomTabController() : binding_(this) {}

ArcCustomTabController::~ArcCustomTabController() = default;

void ArcCustomTabController::BindRequest(
    mojom::ArcCustomTabControllerRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

void ArcCustomTabController::CreateView(int32_t task_id,
                                        int32_t surface_id,
                                        int32_t top_margin,
                                        CreateViewCallback callback) {
  std::move(callback).Run(
      ArcCustomTabView::Create(task_id, surface_id, top_margin));
}

}  // namespace ash
