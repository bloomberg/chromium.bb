// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_util.h"

void CreateShelfItemForDialog(int image_resource_id, aura::Window* window) {
  ash::ShelfItemDetails item_details;
  item_details.type = ash::TYPE_DIALOG;
  item_details.image_resource_id = image_resource_id;
  item_details.title = window->title();
  ash::SetShelfItemDetailsForWindow(window, item_details);
}
