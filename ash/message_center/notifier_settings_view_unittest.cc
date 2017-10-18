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
#include "ui/views/controls/scroll_view.h"

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
    if (!no_notifiers_) {
      notifiers->push_back(NewNotifier("id", "title", true /* enabled */));
      notifiers->push_back(
          NewNotifier("id2", "other title", false /* enabled */));
    }
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

  void set_no_notifiers(bool no_notifiers) { no_notifiers_ = no_notifiers; }

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
  bool no_notifiers_ = false;
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
  const TestingNotifierSettingsProvider* settings_provider() const {
    return &settings_provider_;
  }
  void SetNoNotifiers(bool no_notifiers) {
    settings_provider_.set_no_notifiers(no_notifiers);
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
  SetNoNotifiers(false);
}

void NotifierSettingsViewTest::TearDown() {
  notifier_settings_view_.reset();
  AshTestBase::TearDown();
}

void NotifierSettingsViewTest::InitView() {
  notifier_settings_view_ =
      std::make_unique<NotifierSettingsView>(&settings_provider_);
}

NotifierSettingsView* NotifierSettingsViewTest::GetView() const {
  return notifier_settings_view_.get();
}

TEST_F(NotifierSettingsViewTest, TestLearnMoreButton) {
  InitView();
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

TEST_F(NotifierSettingsViewTest, TestEmptyNotifierView) {
  InitView();
  EXPECT_FALSE(GetView()->no_notifiers_view_->visible());
  EXPECT_TRUE(GetView()->top_label_->visible());

  SetNoNotifiers(true);
  InitView();
  EXPECT_TRUE(GetView()->no_notifiers_view_->visible());
  EXPECT_FALSE(GetView()->top_label_->visible());
}

}  // namespace ash
