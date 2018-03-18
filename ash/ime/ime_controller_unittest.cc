// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include <vector>

#include "ash/ime/test_ime_controller_client.h"
#include "ash/public/interfaces/ime_info.mojom.h"
#include "ash/shell.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {
namespace {

// Refreshes the IME list with fake IMEs and fake menu items.
void RefreshImesWithMenuItems(const std::string& current_ime_id,
                              const std::vector<std::string>& ime_ids,
                              const std::vector<std::string>& menu_item_keys) {
  std::vector<mojom::ImeInfoPtr> available_imes;
  for (const std::string& ime_id : ime_ids) {
    mojom::ImeInfoPtr ime = mojom::ImeInfo::New();
    ime->id = ime_id;
    available_imes.push_back(std::move(ime));
  }
  std::vector<mojom::ImeMenuItemPtr> menu_items;
  for (const std::string& menu_item_key : menu_item_keys) {
    mojom::ImeMenuItemPtr item = mojom::ImeMenuItem::New();
    item->key = menu_item_key;
    menu_items.push_back(std::move(item));
  }
  Shell::Get()->ime_controller()->RefreshIme(
      current_ime_id, std::move(available_imes), std::move(menu_items));
}

// Refreshes the IME list without adding any menu items.
void RefreshImes(const std::string& current_ime_id,
                 const std::vector<std::string>& ime_ids) {
  const std::vector<std::string> empty_menu_items;
  RefreshImesWithMenuItems(current_ime_id, ime_ids, empty_menu_items);
}

class TestImeObserver : public IMEObserver {
 public:
  TestImeObserver() = default;
  ~TestImeObserver() override = default;

  // IMEObserver:
  void OnIMERefresh() override { ++refresh_count_; }
  void OnIMEMenuActivationChanged(bool is_active) override {
    ime_menu_active_ = is_active;
  }

  int refresh_count_ = 0;
  bool ime_menu_active_ = false;
};

class TestImeControllerObserver : public ImeController::Observer {
 public:
  TestImeControllerObserver() = default;

  // IMEController::Observer:
  void OnCapsLockChanged(bool enabled) override { ++caps_lock_count_; }
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override {
    last_keyboard_layout_name_ = layout_name;
  }

  const std::string& last_keyboard_layout_name() const {
    return last_keyboard_layout_name_;
  }

 private:
  int caps_lock_count_ = 0;
  std::string last_keyboard_layout_name_;

  DISALLOW_COPY_AND_ASSIGN(TestImeControllerObserver);
};

using ImeControllerTest = AshTestBase;

TEST_F(ImeControllerTest, RefreshIme) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeObserver observer;
  Shell::Get()->system_tray_notifier()->AddIMEObserver(&observer);

  RefreshImesWithMenuItems("ime1", {"ime1", "ime2"}, {"menu1"});

  // Cached data was updated.
  EXPECT_EQ("ime1", controller->current_ime().id);
  ASSERT_EQ(2u, controller->available_imes().size());
  EXPECT_EQ("ime1", controller->available_imes()[0].id);
  EXPECT_EQ("ime2", controller->available_imes()[1].id);
  ASSERT_EQ(1u, controller->current_ime_menu_items().size());
  EXPECT_EQ("menu1", controller->current_ime_menu_items()[0].key);

  // Observer was notified.
  EXPECT_EQ(1, observer.refresh_count_);
}

TEST_F(ImeControllerTest, NoCurrentIme) {
  ImeController* controller = Shell::Get()->ime_controller();

  // Set up a single IME.
  RefreshImes("ime1", {"ime1"});
  EXPECT_EQ("ime1", controller->current_ime().id);

  // When there is no current IME the cached current IME is empty.
  const std::string empty_ime_id;
  RefreshImes(empty_ime_id, {"ime1"});
  EXPECT_TRUE(controller->current_ime().id.empty());
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

TEST_F(ImeControllerTest, CanSwitchIme) {
  ImeController* controller = Shell::Get()->ime_controller();

  // Can't switch IMEs when none are available.
  ASSERT_EQ(0u, controller->available_imes().size());
  EXPECT_FALSE(controller->CanSwitchIme());

  // Can't switch with only 1 IME.
  RefreshImes("ime1", {"ime1"});
  EXPECT_FALSE(controller->CanSwitchIme());

  // Can switch with more than 1 IME.
  RefreshImes("ime1", {"ime1", "ime2"});
  EXPECT_TRUE(controller->CanSwitchIme());
}

TEST_F(ImeControllerTest, SwitchIme) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;

  // Can't switch IME before the client is set.
  controller->SwitchToNextIme();
  EXPECT_EQ(0, client.next_ime_count_);

  controller->SwitchToPreviousIme();
  EXPECT_EQ(0, client.previous_ime_count_);

  controller->SwitchImeById("ime1", true /* show_message */);
  EXPECT_EQ(0, client.switch_ime_count_);

  // After setting the client the requests are forwarded.
  controller->SetClient(client.CreateInterfacePtr());
  controller->SwitchToNextIme();
  controller->FlushMojoForTesting();
  EXPECT_EQ(1, client.next_ime_count_);

  controller->SwitchToPreviousIme();
  controller->FlushMojoForTesting();
  EXPECT_EQ(1, client.previous_ime_count_);

  controller->SwitchImeById("ime1", true /* show_message */);
  controller->FlushMojoForTesting();
  EXPECT_EQ(1, client.switch_ime_count_);
  EXPECT_EQ("ime1", client.last_switch_ime_id_);
  EXPECT_TRUE(client.last_show_message_);

  controller->SwitchImeById("ime2", false /* show_message */);
  controller->FlushMojoForTesting();
  EXPECT_EQ(2, client.switch_ime_count_);
  EXPECT_EQ("ime2", client.last_switch_ime_id_);
  EXPECT_FALSE(client.last_show_message_);
}

TEST_F(ImeControllerTest, SwitchImeWithAccelerator) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  controller->SetClient(client.CreateInterfacePtr());

  const ui::Accelerator convert(ui::VKEY_CONVERT, ui::EF_NONE);
  const ui::Accelerator non_convert(ui::VKEY_NONCONVERT, ui::EF_NONE);
  const ui::Accelerator wide_half_1(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE);
  const ui::Accelerator wide_half_2(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE);

  // When there are no IMEs available switching by accelerator does not work.
  ASSERT_EQ(0u, controller->available_imes().size());
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(convert));
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(non_convert));
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(wide_half_1));
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(wide_half_2));

  // With only test IMEs (and no Japanese IMEs) the special keys do not work.
  RefreshImes("ime1", {"ime1", "ime2"});
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(convert));
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(non_convert));
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(wide_half_1));
  EXPECT_FALSE(controller->CanSwitchImeWithAccelerator(wide_half_2));

  // Install both a test IME and Japanese IMEs.
  using chromeos::extension_ime_util::GetInputMethodIDByEngineID;
  const std::string nacl_mozc_jp = GetInputMethodIDByEngineID("nacl_mozc_jp");
  const std::string xkb_jp_jpn = GetInputMethodIDByEngineID("xkb:jp::jpn");
  RefreshImes("ime1", {"ime1", nacl_mozc_jp, xkb_jp_jpn});

  // Accelerator based switching now works.
  EXPECT_TRUE(controller->CanSwitchImeWithAccelerator(convert));
  EXPECT_TRUE(controller->CanSwitchImeWithAccelerator(non_convert));
  EXPECT_TRUE(controller->CanSwitchImeWithAccelerator(wide_half_1));
  EXPECT_TRUE(controller->CanSwitchImeWithAccelerator(wide_half_2));

  // Convert keys jump directly to the requested IME.
  controller->SwitchImeWithAccelerator(convert);
  controller->FlushMojoForTesting();
  EXPECT_EQ(1, client.switch_ime_count_);
  EXPECT_EQ(nacl_mozc_jp, client.last_switch_ime_id_);

  controller->SwitchImeWithAccelerator(non_convert);
  controller->FlushMojoForTesting();
  EXPECT_EQ(2, client.switch_ime_count_);
  EXPECT_EQ(xkb_jp_jpn, client.last_switch_ime_id_);

  // Switch from nacl_mozc_jp to xkb_jp_jpn.
  RefreshImes(nacl_mozc_jp, {"ime1", nacl_mozc_jp, xkb_jp_jpn});
  controller->SwitchImeWithAccelerator(wide_half_1);
  controller->FlushMojoForTesting();
  EXPECT_EQ(3, client.switch_ime_count_);
  EXPECT_EQ(xkb_jp_jpn, client.last_switch_ime_id_);

  // Switch from xkb_jp_jpn to nacl_mozc_jp.
  RefreshImes(xkb_jp_jpn, {"ime1", nacl_mozc_jp, xkb_jp_jpn});
  controller->SwitchImeWithAccelerator(wide_half_2);
  controller->FlushMojoForTesting();
  EXPECT_EQ(4, client.switch_ime_count_);
  EXPECT_EQ(nacl_mozc_jp, client.last_switch_ime_id_);
}

TEST_F(ImeControllerTest, SetCapsLock) {
  ImeController* controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  EXPECT_EQ(0, client.set_caps_lock_count_);

  controller->SetCapsLockEnabled(true);
  EXPECT_EQ(0, client.set_caps_lock_count_);

  controller->SetClient(client.CreateInterfacePtr());

  controller->SetCapsLockEnabled(true);
  controller->FlushMojoForTesting();
  EXPECT_EQ(1, client.set_caps_lock_count_);
  // Does not no-op when the state is the same. Should send all notifications.
  controller->SetCapsLockEnabled(true);
  controller->FlushMojoForTesting();
  EXPECT_EQ(2, client.set_caps_lock_count_);
  controller->SetCapsLockEnabled(false);
  controller->FlushMojoForTesting();
  EXPECT_EQ(3, client.set_caps_lock_count_);
  controller->SetCapsLockEnabled(false);
  controller->FlushMojoForTesting();
  EXPECT_EQ(4, client.set_caps_lock_count_);

  EXPECT_FALSE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(true);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);
  EXPECT_FALSE(controller->IsCapsLockEnabled());
}

TEST_F(ImeControllerTest, OnKeyboardLayoutNameChanged) {
  ImeController* controller = Shell::Get()->ime_controller();
  EXPECT_TRUE(controller->keyboard_layout_name().empty());

  TestImeControllerObserver observer;
  controller->AddObserver(&observer);
  controller->OnKeyboardLayoutNameChanged("us(dvorak)");
  EXPECT_EQ("us(dvorak)", controller->keyboard_layout_name());
  EXPECT_EQ("us(dvorak)", observer.last_keyboard_layout_name());
}

}  // namespace
}  // namespace ash
