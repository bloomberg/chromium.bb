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
  virtual void OnWindowDestroying(aura::Window* window) override;
  virtual void OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) override;

  // ShelfDelegate implementation.
  virtual void OnShelfCreated(Shelf* shelf) override;
  virtual void OnShelfDestroyed(Shelf* shelf) override;
  virtual ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  virtual const std::string& GetAppIDForShelfID(ShelfID id) override;
  virtual void PinAppWithID(const std::string& app_id) override;
  virtual bool CanPin() const override;
  virtual bool IsAppPinned(const std::string& app_id) override;
  virtual void UnpinAppWithID(const std::string& app_id) override;

 private:
  static TestShelfDelegate* instance_;

  ShelfModel* model_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELF_DELEGATE_H_
