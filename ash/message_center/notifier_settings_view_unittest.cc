// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/message_center/message_center_controller.h"
#include "ash/message_center/notifier_settings_view.h"
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notifier_id.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

using mojom::NotifierUiData;
using message_center::NotifierId;

namespace {

class TestAshMessageCenterClient : public mojom::AshMessageCenterClient {
 public:
  TestAshMessageCenterClient() : binding_(this) {}
  ~TestAshMessageCenterClient() override = default;

  size_t settings_request_count() const { return settings_request_count_; }
  const NotifierId* last_requested_notifier_id() const {
    return last_notifier_id_settings_requested_.get();
  }
  void set_no_notifiers(bool no_notifiers) { no_notifiers_ = no_notifiers; }

  mojom::AshMessageCenterClientAssociatedPtrInfo CreateInterfacePtr() {
    mojom::AshMessageCenterClientAssociatedPtr ptr;
    binding_.Bind(mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr));
    return ptr.PassInterface();
  }

  // mojom::AshMessageCenterClient:
  void HandleNotificationClosed(const std::string& id, bool by_user) override {}
  void HandleNotificationClicked(const std::string& id) override {}
  void HandleNotificationButtonClicked(const std::string& id,
                                       int button_index) override {}

  void SetNotifierEnabled(const NotifierId& notifier_id,
                          bool enabled) override {}

  void HandleNotifierAdvancedSettingsRequested(
      const NotifierId& notifier_id) override {
    settings_request_count_++;
    last_notifier_id_settings_requested_.reset(new NotifierId(notifier_id));
  }

  void GetNotifierList(GetNotifierListCallback callback) override {
    std::vector<mojom::NotifierUiDataPtr> ui_data;
    if (!no_notifiers_) {
      ui_data.push_back(mojom::NotifierUiData::New(
          NotifierId(NotifierId::APPLICATION, "id"),
          base::ASCIIToUTF16("title"), true /* has_advanced_settings */,
          true /* enabled */, gfx::ImageSkia()));
      ui_data.push_back(mojom::NotifierUiData::New(
          NotifierId(NotifierId::APPLICATION, "id2"),
          base::ASCIIToUTF16("other title"), false /* has_advanced_settings */,
          false /* enabled */, gfx::ImageSkia()));
    }

    std::move(callback).Run(std::move(ui_data));
  }

 private:
  size_t settings_request_count_ = 0u;
  std::unique_ptr<NotifierId> last_notifier_id_settings_requested_;
  bool no_notifiers_ = false;

  mojo::AssociatedBinding<mojom::AshMessageCenterClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestAshMessageCenterClient);
};

}  // namespace

class NotifierSettingsViewTest : public AshTestBase {
 public:
  NotifierSettingsViewTest();
  ~NotifierSettingsViewTest() override;

  void SetUp() override;
  void TearDown() override;

  void InitView();
  NotifierSettingsView* GetView() const;
  TestAshMessageCenterClient* client() { return &client_; }
  void SetNoNotifiers(bool no_notifiers) {
    client_.set_no_notifiers(no_notifiers);
  }

 private:
  TestAshMessageCenterClient client_;
  std::unique_ptr<NotifierSettingsView> notifier_settings_view_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsViewTest);
};

NotifierSettingsViewTest::NotifierSettingsViewTest() = default;

NotifierSettingsViewTest::~NotifierSettingsViewTest() = default;

void NotifierSettingsViewTest::SetUp() {
  AshTestBase::SetUp();
  SetNoNotifiers(false);

  Shell::Get()->message_center_controller()->SetClient(
      client_.CreateInterfacePtr());
}

void NotifierSettingsViewTest::TearDown() {
  notifier_settings_view_.reset();
  AshTestBase::TearDown();
}

void NotifierSettingsViewTest::InitView() {
  notifier_settings_view_.reset();
  notifier_settings_view_ = std::make_unique<NotifierSettingsView>();
}

NotifierSettingsView* NotifierSettingsViewTest::GetView() const {
  return notifier_settings_view_.get();
}

TEST_F(NotifierSettingsViewTest, TestLearnMoreButton) {
  InitView();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  const std::set<NotifierSettingsView::NotifierButton*>& buttons =
      GetView()->buttons_;
  EXPECT_EQ(2u, buttons.size());
  size_t number_of_settings_buttons = 0;
  for (auto* button : buttons) {
    if (button->has_learn_more()) {
      ++number_of_settings_buttons;
      button->SendLearnMorePressedForTest();
    }
  }

  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, number_of_settings_buttons);
  EXPECT_EQ(1u, client()->settings_request_count());
  const NotifierId* last_settings_button_id =
      client()->last_requested_notifier_id();
  ASSERT_FALSE(last_settings_button_id == nullptr);
  EXPECT_EQ(NotifierId(NotifierId::APPLICATION, "id"),
            *last_settings_button_id);
}

TEST_F(NotifierSettingsViewTest, TestEmptyNotifierView) {
  InitView();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetView()->no_notifiers_view_->visible());
  EXPECT_TRUE(GetView()->top_label_->visible());

  SetNoNotifiers(true);
  InitView();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetView()->no_notifiers_view_->visible());
  EXPECT_FALSE(GetView()->top_label_->visible());
}

}  // namespace ash
