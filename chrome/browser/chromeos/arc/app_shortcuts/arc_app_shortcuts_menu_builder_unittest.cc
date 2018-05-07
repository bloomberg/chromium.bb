// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/types/display_constants.h"

namespace arc {

namespace {

constexpr char kFakeAppId[] = "FakeAppId";
constexpr char kFakeAppPackageName[] = "FakeAppPackageName";
constexpr int kLaunchAppShortcutFirst = 100;
constexpr int kLaunchAppShortcutLast = 199;

}  // namespace

class ArcAppShortcutsMenuBuilderTest : public testing::Test {
 public:
  ArcAppShortcutsMenuBuilderTest() = default;
  ~ArcAppShortcutsMenuBuilderTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.SetUp(profile_.get());
    IconDecodeRequest::DisableSafeDecodingForTesting();
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;

  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutsMenuBuilderTest);
};

TEST_F(ArcAppShortcutsMenuBuilderTest, Basic) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::MenuModel> menu;
  auto arc_app_shortcuts_menu_builder =
      std::make_unique<ArcAppShortcutsMenuBuilder>(
          profile(), kFakeAppId, display::kInvalidDisplayId,
          kLaunchAppShortcutFirst, kLaunchAppShortcutLast);
  arc_app_shortcuts_menu_builder->BuildMenu(
      kFakeAppPackageName, std::make_unique<ui::SimpleMenuModel>(nullptr),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::MenuModel> returned_menu) {
            menu = std::move(returned_menu);
            run_loop.Quit();
          }));
  run_loop.Run();

  DCHECK(menu);
  for (int i = 0; i < menu->GetItemCount(); ++i) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", i),
              base::UTF16ToUTF8(menu->GetLabelAt(i)));
  }
}

}  // namespace arc
