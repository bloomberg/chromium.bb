// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/interfaces/ime_controller.mojom.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/fake_input_method_delegate.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

using chromeos::input_method::FakeInputMethodDelegate;
using chromeos::input_method::InputMethodDescriptor;
using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
using chromeos::input_method::MockInputMethodManager;
using ui::ime::InputMethodMenuItem;
using ui::ime::InputMethodMenuManager;

namespace {

// Used to look up IME names.
base::string16 GetLocalizedString(int resource_id) {
  return base::ASCIIToUTF16("localized string");
}

// InputMethodManager with available IMEs.
class TestInputMethodManager : public MockInputMethodManager {
 public:
  class TestState : public MockInputMethodManager::State {
   public:
    TestState() {
      // Set up two input methods.
      std::vector<std::string> layouts({"us"});
      std::vector<std::string> languages({"en-US"});
      InputMethodDescriptor ime1("id1", "name1", "indicator1", layouts,
                                 languages, true /* is_login_keyboard */,
                                 GURL(), GURL());
      InputMethodDescriptor ime2("id2", "name2", "indicator2", layouts,
                                 languages, false /* is_login_keyboard */,
                                 GURL(), GURL());
      current_ime_id_ = ime1.id();
      input_methods_ = {ime1, ime2};
    }

    // MockInputMethodManager::State:
    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override {
      ++change_input_method_count_;
      current_ime_id_ = input_method_id;
      last_show_message_ = show_message;
    }
    std::unique_ptr<std::vector<InputMethodDescriptor>> GetActiveInputMethods()
        const override {
      return base::MakeUnique<std::vector<InputMethodDescriptor>>(
          input_methods_);
    }
    const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const override {
      for (const InputMethodDescriptor& descriptor : input_methods_) {
        if (input_method_id == descriptor.id())
          return &descriptor;
      }
      return nullptr;
    }
    InputMethodDescriptor GetCurrentInputMethod() const override {
      for (const InputMethodDescriptor& descriptor : input_methods_) {
        if (current_ime_id_ == descriptor.id())
          return descriptor;
      }
      return InputMethodUtil::GetFallbackInputMethodDescriptor();
    }
    void SwitchToNextInputMethod() override { ++next_input_method_count_; }
    void SwitchToPreviousInputMethod() override {
      ++previous_input_method_count_;
    }

    std::string current_ime_id_;
    std::vector<InputMethodDescriptor> input_methods_;
    int next_input_method_count_ = 0;
    int previous_input_method_count_ = 0;
    int change_input_method_count_ = 0;
    bool last_show_message_ = false;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override {}

    DISALLOW_COPY_AND_ASSIGN(TestState);
  };

  TestInputMethodManager() : state_(new TestState), util_(&delegate_) {}
  ~TestInputMethodManager() override = default;

  // MockInputMethodManager:
  void AddObserver(InputMethodManager::Observer* observer) override {
    ++add_observer_count_;
  }
  void AddImeMenuObserver(ImeMenuObserver* observer) override {
    ++add_menu_observer_count_;
  }
  void RemoveObserver(InputMethodManager::Observer* observer) override {
    ++remove_observer_count_;
  }
  void RemoveImeMenuObserver(ImeMenuObserver* observer) override {
    ++remove_menu_observer_count_;
  }
  void ActivateInputMethodMenuItem(const std::string& key) override {
    last_activate_menu_item_key_ = key;
  }

  InputMethodUtil* GetInputMethodUtil() override { return &util_; }
  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  scoped_refptr<TestState> state_;
  int add_observer_count_ = 0;
  int remove_observer_count_ = 0;
  int add_menu_observer_count_ = 0;
  int remove_menu_observer_count_ = 0;
  std::string last_activate_menu_item_key_;
  FakeInputMethodDelegate delegate_;
  InputMethodUtil util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

class TestImeController : ash::mojom::ImeController {
 public:
  TestImeController() : binding_(this) {}
  ~TestImeController() override = default;

  // Returns a mojo interface pointer bound to this object.
  ash::mojom::ImeControllerPtr CreateInterfacePtr() {
    ash::mojom::ImeControllerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // ash::mojom::ImeController:
  void SetClient(ash::mojom::ImeControllerClientPtr client) override {}
  void RefreshIme(const std::string& current_ime_id,
                  std::vector<ash::mojom::ImeInfoPtr> available_imes,
                  std::vector<ash::mojom::ImeMenuItemPtr> menu_items) override {
    current_ime_id_ = current_ime_id;
    available_imes_ = std::move(available_imes);
    menu_items_ = std::move(menu_items);
  }
  void SetImesManagedByPolicy(bool managed) override {
    managed_by_policy_ = managed;
  }
  void ShowImeMenuOnShelf(bool show) override {
    show_ime_menu_on_shelf_ = show;
  }
  void SetCapsLockState(bool enabled) override {
    is_caps_lock_enabled_ = enabled;
  }
  void SetExtraInputOptionsEnabledState(bool is_extra_input_options_enabled,
                                        bool is_emoji_enabled,
                                        bool is_handwriting_enabled,
                                        bool is_voice_enabled) override {
    is_extra_input_options_enabled_ = is_extra_input_options_enabled;
    is_emoji_enabled_ = is_emoji_enabled;
    is_handwriting_enabled_ = is_handwriting_enabled;
    is_voice_enabled_ = is_voice_enabled;
  }

  // The most recent values received via mojo.
  std::string current_ime_id_;
  std::vector<ash::mojom::ImeInfoPtr> available_imes_;
  std::vector<ash::mojom::ImeMenuItemPtr> menu_items_;
  bool managed_by_policy_ = false;
  bool show_ime_menu_on_shelf_ = false;
  bool is_caps_lock_enabled_ = false;
  bool is_extra_input_options_enabled_ = false;
  bool is_emoji_enabled_ = false;
  bool is_handwriting_enabled_ = false;
  bool is_voice_enabled_ = false;

 private:
  mojo::Binding<ash::mojom::ImeController> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestImeController);
};

class ImeControllerClientTest : public testing::Test {
 public:
  ImeControllerClientTest() {
    input_method_manager_.delegate_.set_get_localized_string_callback(
        base::Bind(&GetLocalizedString));
  }
  ~ImeControllerClientTest() override = default;

 protected:
  TestInputMethodManager input_method_manager_;

  // Mock of mojo interface in ash.
  TestImeController ime_controller_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ImeControllerClientTest);
};

TEST_F(ImeControllerClientTest, Construction) {
  std::unique_ptr<ImeControllerClient> client =
      base::MakeUnique<ImeControllerClient>(&input_method_manager_);
  client->InitForTesting(ime_controller_.CreateInterfacePtr());
  EXPECT_EQ(1, input_method_manager_.add_observer_count_);
  EXPECT_EQ(1, input_method_manager_.add_menu_observer_count_);

  client.reset();
  EXPECT_EQ(1, input_method_manager_.remove_observer_count_);
  EXPECT_EQ(1, input_method_manager_.remove_menu_observer_count_);
}

TEST_F(ImeControllerClientTest, SetImesManagedByPolicy) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  client.SetImesManagedByPolicy(true);
  client.FlushMojoForTesting();
  EXPECT_TRUE(ime_controller_.managed_by_policy_);
}

TEST_F(ImeControllerClientTest, CapsLock) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  client.OnCapsLockChanged(true);
  client.FlushMojoForTesting();
  EXPECT_TRUE(ime_controller_.is_caps_lock_enabled_);

  client.OnCapsLockChanged(false);
  client.FlushMojoForTesting();
  EXPECT_FALSE(ime_controller_.is_caps_lock_enabled_);
}

TEST_F(ImeControllerClientTest, ExtraInputEnabledStateChange) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  client.OnExtraInputEnabledStateChange(true, true, false, false);
  client.FlushMojoForTesting();
  EXPECT_TRUE(ime_controller_.is_extra_input_options_enabled_);
  EXPECT_TRUE(ime_controller_.is_emoji_enabled_);
  EXPECT_FALSE(ime_controller_.is_handwriting_enabled_);
  EXPECT_FALSE(ime_controller_.is_voice_enabled_);

  client.OnExtraInputEnabledStateChange(true, false, true, true);
  client.FlushMojoForTesting();
  EXPECT_TRUE(ime_controller_.is_extra_input_options_enabled_);
  EXPECT_FALSE(ime_controller_.is_emoji_enabled_);
  EXPECT_TRUE(ime_controller_.is_handwriting_enabled_);
  EXPECT_TRUE(ime_controller_.is_voice_enabled_);

  client.OnExtraInputEnabledStateChange(false, false, false, false);
  client.FlushMojoForTesting();
  EXPECT_FALSE(ime_controller_.is_extra_input_options_enabled_);
  EXPECT_FALSE(ime_controller_.is_emoji_enabled_);
  EXPECT_FALSE(ime_controller_.is_handwriting_enabled_);
  EXPECT_FALSE(ime_controller_.is_voice_enabled_);
}

TEST_F(ImeControllerClientTest, ShowImeMenuOnShelf) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  client.ImeMenuActivationChanged(true);
  client.FlushMojoForTesting();
  EXPECT_TRUE(ime_controller_.show_ime_menu_on_shelf_);
}

TEST_F(ImeControllerClientTest, InputMethodChanged) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  // Simulate a switch to IME 2.
  input_method_manager_.state_->current_ime_id_ = "id2";
  client.InputMethodChanged(&input_method_manager_, nullptr /* profile */,
                            false /* show_message */);
  client.FlushMojoForTesting();

  // IME controller received the change and the list of available IMEs.
  EXPECT_EQ("id2", ime_controller_.current_ime_id_);
  ASSERT_EQ(2u, ime_controller_.available_imes_.size());
  EXPECT_EQ("id1", ime_controller_.available_imes_[0]->id);
  EXPECT_EQ(base::ASCIIToUTF16("name1"),
            ime_controller_.available_imes_[0]->name);
  EXPECT_EQ("id2", ime_controller_.available_imes_[1]->id);
  EXPECT_EQ(base::ASCIIToUTF16("name2"),
            ime_controller_.available_imes_[1]->name);
}

TEST_F(ImeControllerClientTest, NoActiveState) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  input_method_manager_.state_ = nullptr;
  client.InputMethodChanged(&input_method_manager_, nullptr /* profile */,
                            false /* show_message */);
  client.FlushMojoForTesting();
  EXPECT_TRUE(ime_controller_.current_ime_id_.empty());
  EXPECT_TRUE(ime_controller_.available_imes_.empty());
  EXPECT_TRUE(ime_controller_.menu_items_.empty());
}

TEST_F(ImeControllerClientTest, MenuItemChanged) {
  ImeControllerClient client(&input_method_manager_);
  client.InitForTesting(ime_controller_.CreateInterfacePtr());

  const bool is_selection_item = true;
  InputMethodMenuItem item1("key1", "label1", is_selection_item,
                            true /* checked */);
  InputMethodMenuItem item2("key2", "label2", is_selection_item,
                            false /* checked */);

  // Setting the list triggers the InputMethodMenuItemChanged event.
  InputMethodMenuManager::GetInstance()->SetCurrentInputMethodMenuItemList(
      {item1, item2});
  client.FlushMojoForTesting();

  // IME controller received the menu items.
  ASSERT_EQ(2u, ime_controller_.menu_items_.size());
  EXPECT_EQ("key1", ime_controller_.menu_items_[0]->key);
  EXPECT_TRUE(ime_controller_.menu_items_[0]->checked);
  EXPECT_EQ("key2", ime_controller_.menu_items_[1]->key);
  EXPECT_FALSE(ime_controller_.menu_items_[1]->checked);
}

TEST_F(ImeControllerClientTest, SwitchToNextIme) {
  ImeControllerClient client(&input_method_manager_);
  client.SwitchToNextIme();
  EXPECT_EQ(1, input_method_manager_.state_->next_input_method_count_);
}

TEST_F(ImeControllerClientTest, SwitchToPreviousIme) {
  ImeControllerClient client(&input_method_manager_);
  client.SwitchToPreviousIme();
  EXPECT_EQ(1, input_method_manager_.state_->previous_input_method_count_);
}

TEST_F(ImeControllerClientTest, SwitchImeById) {
  ImeControllerClient client(&input_method_manager_);
  client.SwitchImeById("id2", true /* show_message */);
  EXPECT_EQ(1, input_method_manager_.state_->change_input_method_count_);
  EXPECT_EQ("id2", input_method_manager_.state_->current_ime_id_);
  EXPECT_TRUE(input_method_manager_.state_->last_show_message_);

  client.SwitchImeById("id1", false /* show_message */);
  EXPECT_EQ(2, input_method_manager_.state_->change_input_method_count_);
  EXPECT_EQ("id1", input_method_manager_.state_->current_ime_id_);
  EXPECT_FALSE(input_method_manager_.state_->last_show_message_);
}

TEST_F(ImeControllerClientTest, ActivateImeMenuItem) {
  ImeControllerClient client(&input_method_manager_);
  client.ActivateImeMenuItem("key1");
  EXPECT_EQ("key1", input_method_manager_.last_activate_menu_item_key_);
}

}  // namespace
