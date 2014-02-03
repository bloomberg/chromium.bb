// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_item_types.h"

#include "ash/shelf/shelf_constants.h"

namespace ash {

ShelfItem::ShelfItem()
    : type(TYPE_UNDEFINED),
      id(kInvalidShelfID),
      status(STATUS_CLOSED) {
}

ShelfItem::~ShelfItem() {
}

ShelfItemDetails::ShelfItemDetails()
    : type(TYPE_UNDEFINED),
      image_resource_id(kInvalidImageResourceID) {
}

ShelfItemDetails::~ShelfItemDetails() {
}

}  // namespace ash
