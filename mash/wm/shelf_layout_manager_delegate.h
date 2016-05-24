// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_SHELF_LAYOUT_MANAGER_DELEGATE_H_
#define MASH_WM_SHELF_LAYOUT_MANAGER_DELEGATE_H_

namespace mash {
namespace wm {

class ShelfLayoutManagerDelegate {
 public:
  // Called when the window associated with the shelf is available. That is,
  // ShelfLayoutManager::GetShelfWindow() returns non-null.
  virtual void OnShelfWindowAvailable() = 0;

 protected:
  virtual ~ShelfLayoutManagerDelegate() {}
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_SHELF_LAYOUT_MANAGER_DELEGATE_H_
