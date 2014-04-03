// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELF_TEST_API_H_
#define ASH_TEST_SHELF_TEST_API_H_

#include "base/basictypes.h"

namespace ash {

class Shelf;
class ShelfDelegate;
class ShelfView;

namespace test {

// Use the api in this class to access private members of Shelf.
class ShelfTestAPI {
 public:
  explicit ShelfTestAPI(Shelf* shelf);

  ~ShelfTestAPI();

  // An accessor for |shelf_view|.
  ShelfView* shelf_view();

  // Set ShelfDelegate.
  void SetShelfDelegate(ShelfDelegate* delegate);

 private:
  Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SHELF_TEST_API_H_
