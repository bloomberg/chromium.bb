// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shelf_test_api.h"

#include "ash/shelf/shelf.h"

namespace ash {
namespace test {

ShelfTestAPI::ShelfTestAPI(Shelf* shelf)
    : shelf_(shelf) {
}

ShelfTestAPI::~ShelfTestAPI() {
}

ShelfView* ShelfTestAPI::shelf_view() { return shelf_->shelf_view_; }

void ShelfTestAPI::SetShelfDelegate(ShelfDelegate* delegate) {
  shelf_->delegate_ = delegate;
}

}  // namespace test
}  // namespace ash
