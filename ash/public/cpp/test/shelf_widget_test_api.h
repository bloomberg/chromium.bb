// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_SHELF_WIDGET_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_SHELF_WIDGET_TEST_API_H_

#include "ash/ash_export.h"

namespace views {
class View;
}

namespace ash {

// Facilitates browser tests to access shelf widget code.
class ASH_EXPORT ShelfWidgetTestApi {
 public:
  ShelfWidgetTestApi() = default;
  ~ShelfWidgetTestApi() = default;

  views::View* GetHomeButton();
};

}  // namespace ash

#endif
