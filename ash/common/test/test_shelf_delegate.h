// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_TEST_SHELF_DELEGATE_H_
#define ASH_COMMON_TEST_TEST_SHELF_DELEGATE_H_

#include <map>
#include <set>
#include <string>

#include "ash/common/shelf/shelf_delegate.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"

namespace ash {

class WmWindow;

namespace test {

class ShelfInitializer;

// Test implementation of ShelfDelegate.
// Tests may create icons for windows by calling AddShelfItem().
class TestShelfDelegate : public ShelfDelegate, public aura::WindowObserver {
 public:
  TestShelfDelegate();
  ~TestShelfDelegate() override;

  // Adds a ShelfItem for the given |window|. The ShelfItem's status will be
  // STATUS_CLOSED.
  void AddShelfItem(WmWindow* window);

  // Adds a ShelfItem for the given |window| and adds a mapping from the added
  // ShelfItem's ShelfID to the given |app_id|. The ShelfItem's status will be
  // STATUS_CLOSED.
  void AddShelfItem(WmWindow* window, const std::string& app_id);

  // Adds a ShelfItem for the given |window| with the specified |status|.
  void AddShelfItem(WmWindow* window, ShelfItemStatus status);

  // Removes the ShelfItem for the specified |window| and unpins it if it was
  // pinned. The |window|'s ShelfID to app id mapping will be removed if it
  // exists.
  void RemoveShelfItemForWindow(WmWindow* window);

  static TestShelfDelegate* instance() { return instance_; }

  // WindowObserver implementation
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;

  // ShelfDelegate implementation.
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  ShelfID GetShelfIDForAppIDAndLaunchID(const std::string& app_id,
                                        const std::string& launch_id) override;
  bool HasShelfIDToAppIDMapping(ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

 private:
  // Adds a mapping from a ShelfID to an app id.
  void AddShelfIDToAppIDMapping(ShelfID shelf_id, const std::string& app_id);

  // Removes the mapping from a ShelfID to an app id.
  void RemoveShelfIDToAppIDMapping(ShelfID shelf_id);

  static TestShelfDelegate* instance_;

  std::unique_ptr<ShelfInitializer> shelf_initializer_;

  std::set<std::string> pinned_apps_;

  // Tracks the ShelfID to app id mappings.
  std::map<ShelfID, std::string> shelf_id_to_app_id_map_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_COMMON_TEST_TEST_SHELF_DELEGATE_H_
