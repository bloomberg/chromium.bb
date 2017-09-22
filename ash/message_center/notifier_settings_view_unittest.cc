// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/message_center/notifier_settings_view.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notifier_settings.h"

using message_center::Notifier;
using message_center::NotifierGroup;
using message_center::NotifierId;
using message_center::NotifierSettingsObserver;
using message_center::NotifierSettingsProvider;

namespace ash {

namespace {

// A class used by NotifierSettingsView to integrate with a setting system
// for the clients of this module.
class TestingNotifierSettingsProvider : public NotifierSettingsProvider {
 public:
  ~TestingNotifierSettingsProvider() override = default;

  size_t request_count() const { return request_count_; }
  const NotifierId* last_requested_notifier_id() const {
    return last_notifier_id_settings_requested_.get();
  }

  // NotifierSettingsProvider

  void AddObserver(NotifierSettingsObserver* observer) override {}
  void RemoveObserver(NotifierSettingsObserver* observer) override {}

  size_t GetNotifierGroupCount() const override { return 1; }

  const message_center::NotifierGroup& GetNotifierGroupAt(
      size_t index) const override {
    DCHECK_EQ(0u, index);
    return GetActiveNotifierGroup();
  }

  bool IsNotifierGroupActiveAt(size_t index) const override {
    return index == 0;
  }

  void SwitchToNotifierGroup(size_t index) override { NOTREACHED(); }

  const NotifierGroup& GetActiveNotifierGroup() const override {
    static NotifierGroup group{base::UTF8ToUTF16("Fake name"),
                               base::UTF8ToUTF16("fake@email.com")};
    return group;
  }

  void GetNotifierList(
      std::vector<std::unique_ptr<Notifier>>* notifiers) override {
    notifiers->clear();
    notifiers->push_back(NewNotifier("id", "title", /*enabled=*/true));
    notifiers->push_back(NewNotifier("id2", "other title", /*enabled=*/false));
  }

  void SetNotifierEnabled(const NotifierId& notifier_id,
                          bool enabled) override {}

  // Called when the settings window is closed.
  void OnNotifierSettingsClosing() override {}

  bool NotifierHasAdvancedSettings(
      const NotifierId& notifier_id) const override {
    return notifier_id == NotifierId(NotifierId::APPLICATION, "id");
  }

  void OnNotifierAdvancedSettingsRequested(
      const NotifierId& notifier_id,
      const std::string* notification_id) override {
    request_count_++;
    last_notifier_id_settings_requested_.reset(new NotifierId(notifier_id));
  }

 private:
  std::unique_ptr<Notifier> NewNotifier(const std::string& id,
                                        const std::string& title,
                                        bool enabled) {
    NotifierId notifier_id(NotifierId::APPLICATION, id);
    return std::make_unique<Notifier>(notifier_id, base::UTF8ToUTF16(title),
                                      enabled);
  }

  size_t request_count_ = 0u;
  std::unique_ptr<NotifierId> last_notifier_id_settings_requested_;
};

}  // namespace

class NotifierSettingsViewTest : public AshTestBase {
 public:
  NotifierSettingsViewTest();
  ~NotifierSettingsViewTest() override;

  void SetUp() override;
  void TearDown() override;

  NotifierSettingsView* GetView() const;
  const TestingNotifierSettingsProvider* settings_provider() const {
    return &settings_provider_;
  }

 private:
  TestingNotifierSettingsProvider settings_provider_;
  std::unique_ptr<NotifierSettingsView> notifier_settings_view_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsViewTest);
};

NotifierSettingsViewTest::NotifierSettingsViewTest() = default;

NotifierSettingsViewTest::~NotifierSettingsViewTest() = default;

void NotifierSettingsViewTest::SetUp() {
  AshTestBase::SetUp();
  notifier_settings_view_ =
      std::make_unique<NotifierSettingsView>(&settings_provider_);
}

void NotifierSettingsViewTest::TearDown() {
  notifier_settings_view_.reset();
  AshTestBase::TearDown();
}

NotifierSettingsView* NotifierSettingsViewTest::GetView() const {
  return notifier_settings_view_.get();
}

TEST_F(NotifierSettingsViewTest, TestLearnMoreButton) {
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

  EXPECT_EQ(1u, number_of_settings_buttons);
  EXPECT_EQ(1u, settings_provider()->request_count());
  const NotifierId* last_settings_button_id =
      settings_provider()->last_requested_notifier_id();
  ASSERT_FALSE(last_settings_button_id == nullptr);
  EXPECT_EQ(NotifierId(NotifierId::APPLICATION, "id"),
            *last_settings_button_id);
}

}  // namespace ash
