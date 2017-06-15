// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include <vector>

#include "ash/public/interfaces/ime_info.mojom.h"
#include "ash/shell.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace {

class TestImeObserver : public IMEObserver {
 public:
  TestImeObserver() = default;
  ~TestImeObserver() override = default;

  // IMEObserver:
  void OnIMERefresh() override { refresh_count_++; }
  void OnIMEMenuActivationChanged(bool is_active) override {
    ime_menu_active_ = is_active;
  }

  int refresh_count_ = 0;
  bool ime_menu_active_ = false;
};

using ImeControllerTest = test::AshTestBase;

TEST_F(ImeControllerTest, RefreshIme) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeObserver observer;
  Shell::Get()->system_tray_notifier()->AddIMEObserver(&observer);

  mojom::ImeInfo ime1;
  ime1.id = "ime1";
  mojom::ImeInfo ime2;
  ime2.id = "ime2";

  mojom::ImeMenuItem item1;
  item1.key = "key1";

  controller->RefreshIme(ime1, {ime1, ime2}, {item1});

  // Cached data was updated.
  EXPECT_EQ("ime1", controller->current_ime().id);
  ASSERT_EQ(2u, controller->available_imes().size());
  EXPECT_EQ("ime1", controller->available_imes()[0].id);
  EXPECT_EQ("ime2", controller->available_imes()[1].id);
  ASSERT_EQ(1u, controller->current_ime_menu_items().size());
  EXPECT_EQ("key1", controller->current_ime_menu_items()[0].key);

  // Observer was notified.
  EXPECT_EQ(1, observer.refresh_count_);
}

TEST_F(ImeControllerTest, SetImesManagedByPolicy) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeObserver observer;
  Shell::Get()->system_tray_notifier()->AddIMEObserver(&observer);

  // Defaults to false.
  EXPECT_FALSE(controller->managed_by_policy());

  // Setting the value notifies observers.
  controller->SetImesManagedByPolicy(true);
  EXPECT_TRUE(controller->managed_by_policy());
  EXPECT_EQ(1, observer.refresh_count_);
}

TEST_F(ImeControllerTest, ShowImeMenuOnShelf) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeObserver observer;
  Shell::Get()->system_tray_notifier()->AddIMEObserver(&observer);

  controller->ShowImeMenuOnShelf(true);
  EXPECT_TRUE(observer.ime_menu_active_);
}

}  // namespace
}  // namespace ash
