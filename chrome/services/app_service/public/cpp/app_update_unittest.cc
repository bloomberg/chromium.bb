// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const apps::mojom::AppType app_type = apps::mojom::AppType::kArc;
const char app_id[] = "abcdefgh";
const char test_name_0[] = "Inigo Montoya";
const char test_name_1[] = "Dread Pirate Roberts";
}  // namespace

class AppUpdateTest : public testing::Test {
 protected:
  apps::mojom::Readiness expect_readiness_;
  bool expect_readiness_changed_;

  std::string expect_name_;
  bool expect_name_changed_;

  apps::mojom::IconKeyPtr expect_icon_key_;
  bool expect_icon_key_changed_;

  std::vector<apps::mojom::PermissionPtr> expect_permissions_;
  bool expect_permissions_changed_;

  apps::mojom::OptionalBool expect_installed_internally_;
  bool expect_installed_internally_changed_;

  apps::mojom::OptionalBool expect_show_in_launcher_;
  bool expect_show_in_launcher_changed_;

  apps::mojom::OptionalBool expect_show_in_search_;
  bool expect_show_in_search_changed_;

  static constexpr uint32_t kPermissionTypeLocation = 100;
  static constexpr uint32_t kPermissionTypeNotification = 200;

  apps::mojom::PermissionPtr MakePermission(uint32_t permission_id,
                                            apps::mojom::TriState value) {
    apps::mojom::PermissionPtr permission = apps::mojom::Permission::New();
    permission->permission_id = permission_id;
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(value);
    return permission;
  }

  void ExpectNoChange() {
    expect_readiness_changed_ = false;
    expect_name_changed_ = false;
    expect_icon_key_changed_ = false;
    expect_permissions_changed_ = false;
    expect_installed_internally_changed_ = false;
    expect_show_in_launcher_changed_ = false;
    expect_show_in_search_changed_ = false;
  }

  void CheckExpects(const apps::AppUpdate& u) {
    EXPECT_EQ(expect_readiness_, u.Readiness());
    EXPECT_EQ(expect_readiness_changed_, u.ReadinessChanged());

    EXPECT_EQ(expect_name_, u.Name());
    EXPECT_EQ(expect_name_changed_, u.NameChanged());

    EXPECT_EQ(expect_icon_key_, u.IconKey());
    EXPECT_EQ(expect_icon_key_changed_, u.IconKeyChanged());

    EXPECT_EQ(expect_permissions_, u.Permissions());
    EXPECT_EQ(expect_permissions_changed_, u.PermissionsChanged());

    EXPECT_EQ(expect_installed_internally_, u.InstalledInternally());
    EXPECT_EQ(expect_installed_internally_changed_,
              u.InstalledInternallyChanged());

    EXPECT_EQ(expect_show_in_launcher_, u.ShowInLauncher());
    EXPECT_EQ(expect_show_in_launcher_changed_, u.ShowInLauncherChanged());

    EXPECT_EQ(expect_show_in_search_, u.ShowInSearch());
    EXPECT_EQ(expect_show_in_search_changed_, u.ShowInSearchChanged());
  }

  void TestAppUpdate(apps::mojom::App* state, apps::mojom::App* delta) {
    apps::AppUpdate u(state, delta);

    EXPECT_EQ(app_type, u.AppType());
    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());

    expect_readiness_ = apps::mojom::Readiness::kUnknown;
    expect_name_ = "";
    expect_icon_key_ = nullptr;
    expect_permissions_.clear();
    expect_installed_internally_ = apps::mojom::OptionalBool::kUnknown;
    expect_show_in_launcher_ = apps::mojom::OptionalBool::kUnknown;
    expect_show_in_search_ = apps::mojom::OptionalBool::kUnknown;
    ExpectNoChange();
    CheckExpects(u);

    if (delta) {
      delta->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      state->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::mojom::Readiness::kReady;
      expect_readiness_ = apps::mojom::Readiness::kReady;
      expect_readiness_changed_ = true;
      CheckExpects(u);

      delta->name = base::nullopt;
      expect_name_ = state ? test_name_0 : "";
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::mojom::Readiness::kDisabledByPolicy;
      expect_readiness_ = apps::mojom::Readiness::kDisabledByPolicy;
      expect_readiness_changed_ = true;
      delta->name = test_name_1;
      expect_name_ = test_name_1;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    // IconKey tests.

    if (state) {
      auto x = apps::mojom::IconKey::New(apps::mojom::IconType::kResource, 100,
                                         std::string());
      state->icon_key = x.Clone();
      expect_icon_key_ = x.Clone();
      expect_icon_key_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      auto x = apps::mojom::IconKey::New(apps::mojom::IconType::kExtension, 0,
                                         "one hundred");
      delta->icon_key = x.Clone();
      expect_icon_key_ = x.Clone();
      expect_icon_key_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstalledInternally tests.

    if (state) {
      state->installed_internally = apps::mojom::OptionalBool::kFalse;
      expect_installed_internally_ = apps::mojom::OptionalBool::kFalse;
      expect_installed_internally_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->installed_internally = apps::mojom::OptionalBool::kTrue;
      expect_installed_internally_ = apps::mojom::OptionalBool::kTrue;
      expect_installed_internally_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInSearch tests.

    if (state) {
      state->show_in_search = apps::mojom::OptionalBool::kFalse;
      expect_show_in_search_ = apps::mojom::OptionalBool::kFalse;
      expect_show_in_search_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_search = apps::mojom::OptionalBool::kTrue;
      expect_show_in_search_ = apps::mojom::OptionalBool::kTrue;
      expect_show_in_search_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Permission tests.

    if (state) {
      auto p0 = MakePermission(kPermissionTypeLocation,
                               apps::mojom::TriState::kAllow);
      auto p1 = MakePermission(kPermissionTypeNotification,
                               apps::mojom::TriState::kAllow);
      state->permissions.push_back(p0.Clone());
      state->permissions.push_back(p1.Clone());
      expect_permissions_.push_back(p0.Clone());
      expect_permissions_.push_back(p1.Clone());
      expect_permissions_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_permissions_.clear();
      auto p0 = MakePermission(kPermissionTypeNotification,
                               apps::mojom::TriState::kAllow);
      auto p1 = MakePermission(kPermissionTypeLocation,
                               apps::mojom::TriState::kBlock);

      delta->permissions.push_back(p0.Clone());
      delta->permissions.push_back(p1.Clone());
      expect_permissions_.push_back(p0.Clone());
      expect_permissions_.push_back(p1.Clone());
      expect_permissions_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
  }
};

TEST_F(AppUpdateTest, StateIsNonNull) {
  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  TestAppUpdate(state.get(), nullptr);
}

TEST_F(AppUpdateTest, DeltaIsNonNull) {
  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  TestAppUpdate(nullptr, delta.get());
}

TEST_F(AppUpdateTest, BothAreNonNull) {
  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  TestAppUpdate(state.get(), delta.get());
}
