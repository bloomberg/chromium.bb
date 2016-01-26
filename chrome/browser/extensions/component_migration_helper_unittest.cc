// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/values.h"
#include "chrome/browser/extensions/component_migration_helper.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace extensions {
namespace {

const char kTestActionId[] = "toolbar-action";

class MockComponentActionDelegate
    : public ComponentMigrationHelper::ComponentActionDelegate {
 public:
  MOCK_METHOD1(AddComponentAction, void(const std::string&));
  MOCK_METHOD1(RemoveComponentAction, void(const std::string&));
  MOCK_CONST_METHOD1(HasComponentAction, bool(const std::string&));
};

class MockComponentMigrationHelper : public ComponentMigrationHelper {
 public:
  MockComponentMigrationHelper(Profile* profile,
                               ComponentActionDelegate* delegate)
      : ComponentMigrationHelper(profile, delegate) {}

  ~MockComponentMigrationHelper() override{};

  void SetTestComponentActionPref(bool enabled) {
    SetComponentActionPref(kTestActionId, enabled);
  }

  void EnableTestFeature() { enabled_actions_.insert(kTestActionId); }

  void DisableTestFeature() { enabled_actions_.erase(kTestActionId); }
};

}  // namespace

class ComponentMigrationHelperTest : public ExtensionServiceTestBase {
 protected:
  ComponentMigrationHelperTest() {}
  ~ComponentMigrationHelperTest() override {}

  void SetUp() override {
    extension_action_redesign_.reset(new FeatureSwitch::ScopedOverride(
        FeatureSwitch::extension_action_redesign(),
        FeatureSwitch::OVERRIDE_ENABLED));

    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    migrated_extension_a_ = extension_action_test_util::CreateActionExtension(
        "migrated_browser_action_a",
        extension_action_test_util::BROWSER_ACTION);

    migrated_extension_b_ = extension_action_test_util::CreateActionExtension(
        "migrated_browser_action_b",
        extension_action_test_util::BROWSER_ACTION);

    unregistered_extension_ = extension_action_test_util::CreateActionExtension(
        "unregistered_extension", extension_action_test_util::BROWSER_ACTION);

    mock_helper_.reset(new StrictMock<MockComponentMigrationHelper>(
        profile(), &mock_delegate_));
    mock_helper_->Register(kTestActionId, migrated_extension_a_->id());
    mock_helper_->Register(kTestActionId, migrated_extension_b_->id());
  }

  bool IsTestComponentActionEnabled() {
    const base::DictionaryValue* migration_pref =
        profile()->GetPrefs()->GetDictionary(
            ::prefs::kToolbarMigratedComponentActionStatus);
    if (!migration_pref->HasKey(kTestActionId))
      return false;
    bool enable_value = false;
    CHECK(migration_pref->GetBoolean(kTestActionId, &enable_value));
    return enable_value;
  }

  StrictMock<MockComponentActionDelegate> mock_delegate_;
  scoped_ptr<StrictMock<MockComponentMigrationHelper>> mock_helper_;
  scoped_ptr<FeatureSwitch::ScopedOverride> extension_action_redesign_;

  // Migrated extensions with browser actions.
  scoped_refptr<const Extension> migrated_extension_a_;
  scoped_refptr<const Extension> migrated_extension_b_;
  // An extension that is not migrated.
  scoped_refptr<const Extension> unregistered_extension_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentMigrationHelperTest);
};

TEST_F(ComponentMigrationHelperTest, FeatureEnabledWhenExtensionInstalled) {
  service()->AddExtension(migrated_extension_a_.get());
  service()->AddExtension(migrated_extension_b_.get());

  EXPECT_CALL(mock_delegate_, HasComponentAction(kTestActionId))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_delegate_, AddComponentAction(kTestActionId));

  mock_helper_->OnFeatureEnabled(kTestActionId);
  EXPECT_TRUE(IsTestComponentActionEnabled());
  EXPECT_FALSE(
      registry()->enabled_extensions().Contains(migrated_extension_a_->id()));
  EXPECT_FALSE(
      registry()->enabled_extensions().Contains(migrated_extension_b_->id()));
}

TEST_F(ComponentMigrationHelperTest, FeatureEnabledWithNoPref) {
  mock_helper_->OnFeatureEnabled(kTestActionId);
  EXPECT_FALSE(IsTestComponentActionEnabled());
}

TEST_F(ComponentMigrationHelperTest, FeatureEnabledWithPrefFalse) {
  mock_helper_->SetTestComponentActionPref(false);

  mock_helper_->OnFeatureEnabled(kTestActionId);
  EXPECT_FALSE(IsTestComponentActionEnabled());
}

TEST_F(ComponentMigrationHelperTest, FeatureEnabledWithPrefTrue) {
  mock_helper_->SetTestComponentActionPref(true);

  EXPECT_CALL(mock_delegate_, HasComponentAction(kTestActionId))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_delegate_, AddComponentAction(kTestActionId));

  mock_helper_->OnFeatureEnabled(kTestActionId);
  EXPECT_TRUE(IsTestComponentActionEnabled());
}

TEST_F(ComponentMigrationHelperTest, FeatureDisabledWithAction) {
  mock_helper_->EnableTestFeature();

  EXPECT_CALL(mock_delegate_, HasComponentAction(kTestActionId))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_delegate_, RemoveComponentAction(kTestActionId));

  mock_helper_->OnFeatureDisabled(kTestActionId);
  EXPECT_FALSE(IsTestComponentActionEnabled());
}

TEST_F(ComponentMigrationHelperTest, InstallWithFeatureEnabled) {
  mock_helper_->EnableTestFeature();

  EXPECT_CALL(mock_delegate_, HasComponentAction(kTestActionId))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_delegate_, AddComponentAction(kTestActionId));

  service()->AddExtension(migrated_extension_a_.get());
  // The test framework does not call OnExtensionReady :-/
  mock_helper_->OnExtensionReady(browser_context(),
                                 migrated_extension_a_.get());

  EXPECT_TRUE(IsTestComponentActionEnabled());
  EXPECT_FALSE(
      registry()->enabled_extensions().Contains(migrated_extension_a_->id()));
}

TEST_F(ComponentMigrationHelperTest, InstallWithFeatureDisabled) {
  mock_helper_->DisableTestFeature();
  service()->AddExtension(migrated_extension_a_.get());
  EXPECT_FALSE(IsTestComponentActionEnabled());
  EXPECT_TRUE(
      registry()->enabled_extensions().Contains(migrated_extension_a_->id()));
}

TEST_F(ComponentMigrationHelperTest, InstallUnregisteredExtension) {
  service()->AddExtension(unregistered_extension_.get());
  EXPECT_FALSE(IsTestComponentActionEnabled());
  EXPECT_TRUE(
      registry()->enabled_extensions().Contains(unregistered_extension_->id()));
}

TEST_F(ComponentMigrationHelperTest, RemoveComponentAction) {
  mock_helper_->SetTestComponentActionPref(true);

  EXPECT_CALL(mock_delegate_, HasComponentAction(kTestActionId))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_delegate_, AddComponentAction(kTestActionId));

  mock_helper_->OnFeatureEnabled(kTestActionId);
  EXPECT_TRUE(IsTestComponentActionEnabled());

  EXPECT_CALL(mock_delegate_, HasComponentAction(kTestActionId))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_delegate_, RemoveComponentAction(kTestActionId));

  mock_helper_->OnActionRemoved(kTestActionId);
  EXPECT_FALSE(IsTestComponentActionEnabled());
}

}  // namespace extensions
