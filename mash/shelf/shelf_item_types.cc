// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shelf/shelf_item_types.h"

#include "mash/shelf/shelf_constants.h"

namespace mash {
namespace shelf {

ShelfItem::ShelfItem()
    : type(TYPE_UNDEFINED),
      id(kInvalidShelfID),
      status(STATUS_CLOSED),
      window_id(0) {}

ShelfItem::~ShelfItem() {}

}  // namespace shelf
}  // namespace mash
