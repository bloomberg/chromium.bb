// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"

#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using testing::ByRef;
using testing::Eq;

class ArcAppReinstallSearchProviderTest : public AppListTestBase {
 protected:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_app_test_.SetUp(profile_.get());
    app_provider_ =
        base::WrapUnique(new app_list::ArcAppReinstallSearchProvider(
            profile_.get(), /*max_result_count=*/2));

    std::unique_ptr<base::MockRepeatingTimer> timer =
        std::make_unique<base::MockRepeatingTimer>();
    mock_timer_ = timer.get();
    app_provider_->SetTimerForTesting(std::move(timer));
    app_candidate_ptr_ = arc::mojom::AppReinstallCandidate::New();
  }

  void TearDown() override {
    mock_timer_ = nullptr;
    app_provider_.reset(nullptr);
    arc_app_test_.TearDown();
    AppListTestBase::TearDown();
  }

  arc::FakeAppInstance* app_instance() { return arc_app_test_.app_instance(); }

  void SendPlayStoreApp() {
    arc::mojom::AppInfo app;
    app.name = "Play Store";
    app.package_name = arc::kPlayStorePackage;
    app.activity = arc::kPlayStoreActivity;

    app_instance()->RefreshAppList();
    app_instance()->SendRefreshAppList({app});
  }

  arc::mojom::ArcPackageInfoPtr GetPackagePtr(const std::string& package_name) {
    arc::mojom::ArcPackageInfo package;
    package.package_name = package_name;
    package.sync = false;
    return package.Clone();
  }

  // Owned by |app_provider_|.
  base::MockRepeatingTimer* mock_timer_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<app_list::ArcAppReinstallSearchProvider> app_provider_;
  arc::mojom::AppReinstallCandidatePtr app_candidate_ptr_;
};

namespace {
class TestSearchResult : public ChromeSearchResult {
 public:
  void Open(int event_flags) override {}
  void SetId(const std::string& str) {
    // set_id is protected in chromesearchresult.
    ChromeSearchResult::set_id(str);
  }
};
}  // namespace

TEST_F(ArcAppReinstallSearchProviderTest, NoResultWithoutPlayStore) {
  EXPECT_EQ(0u, app_provider_->results().size());
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestTimerOn) {
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
  mock_timer_->Fire();
  EXPECT_EQ(2, app_instance()->get_app_reinstall_callback_count());
  // We expect no results since there are no apps we added to the candidate
  // list.
  EXPECT_EQ(0u, app_provider_->results().size());

  // Now, stop!
  arc_app_test_.StopArcInstance();
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestResultsWithSearchChanged) {
  std::vector<arc::mojom::AppReinstallCandidatePtr> candidates;
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage1", "Title of first package",
      "http://icon.com/icon1", 15, 4.7));
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage2", "Title of second package",
      "http://icon.com/icon2", 15, 4.3));
  app_instance()->SetAppReinstallCandidates(candidates);
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
  // So we should have populated exactly 0 apps, since we expect loading to
  // finish later.
  EXPECT_EQ(0u, app_provider_->results().size());
  EXPECT_EQ(2u, app_provider_->loading_icon_urls_.size());

  // Fake the load of 2 icons.
  app_provider_->OnIconLoaded("http://icon.com/icon1");
  EXPECT_EQ(1u, app_provider_->loading_icon_urls_.size());
  app_provider_->OnIconLoaded("http://icon.com/icon2");
  ASSERT_EQ(2u, app_provider_->results().size());

  EXPECT_EQ(
      "https://play.google.com/store/apps/details?id=com.package.fakepackage1",
      app_provider_->results()[0]->id());

  // Test that we set to 0 results when having a query, or when arc is turned
  // off.
  app_provider_->Start(base::UTF8ToUTF16("non empty query"));
  EXPECT_EQ(0u, app_provider_->results().size());
  // Verify that all icons are still loaded.
  EXPECT_EQ(2u, app_provider_->icon_urls_.size());
  EXPECT_EQ(0u, app_provider_->loading_icon_urls_.size());
  app_provider_->Start(base::string16());
  EXPECT_EQ(2u, app_provider_->results().size());

  app_instance()->SendInstallationStarted("com.package.fakepackage1");
  EXPECT_EQ(1u, app_provider_->results().size());

  // We should see that only 1 icon is loaded in our final results.
  EXPECT_EQ(1u, app_provider_->icon_urls_.size());

  // When arc is turned off, we should get no results and that we get no
  arc_app_test_.StopArcInstance();
  EXPECT_EQ(0u, app_provider_->results().size());
  // We expect icons to be deleted when arc is stopped.
  EXPECT_EQ(0u, app_provider_->icon_urls_.size());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestResultsWithAppsChanged) {
  std::vector<arc::mojom::AppReinstallCandidatePtr> candidates;
  // We do something unlikely and sneaky here: we give the same icon to two
  // apps. Unlikely, but possible.
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage1", "Title of first package",
      "http://icon.com/icon1", 15, 4.7));
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage2", "Title of second package",
      "http://icon.com/icon1", 15, 4.3));
  app_instance()->SetAppReinstallCandidates(candidates);
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
  // So we should have populated exactly 0 apps, since we expect loading to
  // finish later.
  EXPECT_EQ(0u, app_provider_->results().size());

  // Fake the load of 1 icon.
  app_provider_->OnIconLoaded("http://icon.com/icon1");
  ASSERT_EQ(2u, app_provider_->results().size());

  EXPECT_EQ(
      "https://play.google.com/store/apps/details?id=com.package.fakepackage1",
      app_provider_->results()[0]->id());

  app_instance()->SendInstallationStarted("com.package.fakepackage1");
  EXPECT_EQ(1u, app_provider_->results().size());
  app_instance()->InstallPackage(GetPackagePtr("com.package.fakepackage1"));
  app_instance()->SendInstallationFinished("com.package.fakepackage1", true);
  EXPECT_EQ(1u, app_provider_->results().size());
  app_instance()->UninstallPackage("com.package.fakepackage1");
  EXPECT_EQ(2u, app_provider_->results().size());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestResultListComparison) {
  std::vector<std::unique_ptr<ChromeSearchResult>> a, b;
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  a.emplace_back(new TestSearchResult);

  // different sizes.
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b.emplace_back(new TestSearchResult);
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  // Different Titles.
  a[0]->SetTitle(base::UTF8ToUTF16("fake_title"));
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->SetTitle(base::UTF8ToUTF16("fake_title"));
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // Different ID.
  static_cast<TestSearchResult*>(a[0].get())->SetId("id");
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  static_cast<TestSearchResult*>(b[0].get())->SetId(a[0]->id());
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // Different relevance
  a[0]->set_relevance(0.7);
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->set_relevance(a[0]->relevance());
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // Different Rating.
  a[0]->SetRating(0.3);
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->SetRating(a[0]->rating());
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // for icon equality.
  gfx::ImageSkia aimg =
                     *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                         IDR_APP_DEFAULT_ICON),
                 bimg =
                     *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                         IDR_EXTENSION_DEFAULT_ICON);
  ASSERT_FALSE(aimg.BackedBySameObjectAs(bimg));
  a[0]->SetIcon(aimg);
  b[0]->SetIcon(bimg);
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->SetIcon(aimg);
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
}
