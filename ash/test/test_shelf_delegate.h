// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELF_DELEGATE_H_
#define ASH_TEST_TEST_SHELF_DELEGATE_H_

#include <map>
#include <set>

#include "ash/shelf/shelf_delegate.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"

namespace ash {

class ShelfModel;

namespace test {

// Test implementation of ShelfDelegate.
// Tests may create icons for windows by calling AddShelfItem().
class TestShelfDelegate : public ShelfDelegate, public aura::WindowObserver {
 public:
  explicit TestShelfDelegate(ShelfModel* model);
  virtual ~TestShelfDelegate();

  void AddShelfItem(aura::Window* window);
  void AddShelfItem(aura::Window* window, ShelfItemStatus status);
  void RemoveShelfItemForWindow(aura::Window* window);

  static TestShelfDelegate* instance() { return instance_; }

  // WindowObserver implementation
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) OVERRIDE;

  // ShelfDelegate implementation.
  virtual void OnShelfCreated(Shelf* shelf) OVERRIDE;
  virtual void OnShelfDestroyed(Shelf* shelf) OVERRIDE;
  virtual ShelfID GetShelfIDForAppID(const std::string& app_id) OVERRIDE;
  virtual const std::string& GetAppIDForShelfID(ShelfID id) OVERRIDE;
  virtual void PinAppWithID(const std::string& app_id) OVERRIDE;
  virtual bool CanPin() const OVERRIDE;
  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE;
  virtual void UnpinAppWithID(const std::string& app_id) OVERRIDE;

 private:
  static TestShelfDelegate* instance_;

  ShelfModel* model_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELF_DELEGATE_H_
