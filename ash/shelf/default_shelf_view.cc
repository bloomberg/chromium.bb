// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/default_shelf_view.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"

namespace ash {

DefaultShelfView::DefaultShelfView(ShelfModel* model,
                                   Shelf* shelf,
                                   ShelfWidget* shelf_widget)
    : ShelfView(model, shelf, shelf_widget) {}

DefaultShelfView::~DefaultShelfView() = default;

}  // namespace ash
