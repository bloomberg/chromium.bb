// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace {

const char* kWebApplicationInfoTitle = "Meta Title";
const char* kDefaultManifestUrl = "https://www.example.com/manifest.json";
const char* kDefaultIconUrl = "https://www.example.com/icon.png";
const char* kDefaultManifestName = "Default Name";
const char* kDefaultManifestShortName = "Default Short Name";
const char* kDefaultStartUrl = "https://www.example.com/index.html";
const blink::WebDisplayMode kDefaultManifestDisplayMode =
    blink::kWebDisplayModeStandalone;
const int kIconSizePx = 144;

// Tracks which of the AddToHomescreenDataFetcher::Observer methods have been
// called.
class ObserverWaiter : public AddToHomescreenDataFetcher::Observer {
 public:
  ObserverWaiter()
      : is_webapk_compatible_(false),
        determined_webapk_compatibility_(false),
        title_available_(false),
        data_available_(false) {}
  ~ObserverWaiter() override {}

  // Waits till the OnDataAvailable() callback is called.
  void WaitForDataAvailable() {
    if (data_available_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnDidDetermineWebApkCompatibility(bool is_webapk_compatible) override {
    // This should only be called once.
    EXPECT_FALSE(determined_webapk_compatibility_);
    EXPECT_FALSE(title_available_);
    determined_webapk_compatibility_ = true;
    is_webapk_compatible_ = is_webapk_compatible;
  }

  void OnUserTitleAvailable(const base::string16& title,
                            const GURL& url) override {
    // This should only be called once.
    EXPECT_FALSE(title_available_);
    EXPECT_FALSE(data_available_);
    title_available_ = true;
    title_ = title;
  }

  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& primary_icon,
                       const SkBitmap& badge_icon) override {
    // This should only be called once.
    EXPECT_FALSE(data_available_);
    EXPECT_TRUE(title_available_);
    data_available_ = true;
    if (!quit_closure_.is_null())
      quit_closure_.Run();
  }

  base::string16 title() const { return title_; }
  bool is_webapk_compatible() const { return is_webapk_compatible_; }
  bool determined_webapk_compatibility() const {
    return determined_webapk_compatibility_;
  }
  bool title_available() const { return title_available_; }

 private:
  base::string16 title_;
  bool is_webapk_compatible_;
  bool determined_webapk_compatibility_;
  bool title_available_;
  bool data_available_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ObserverWaiter);
};

// Builds non-null base::NullableString16 from a UTF8 string.
base::NullableString16 NullableStringFromUTF8(const std::string& value) {
  return base::NullableString16(base::UTF8ToUTF16(value), false);
}

// Builds WebAPK compatible content::Manifest.
content::Manifest BuildDefaultManifest() {
  content::Manifest manifest;
  manifest.name = NullableStringFromUTF8(kDefaultManifestName);
  manifest.short_name = NullableStringFromUTF8(kDefaultManifestShortName);
  manifest.start_url = GURL(kDefaultStartUrl);
  manifest.display = kDefaultManifestDisplayMode;

  content::Manifest::Icon primary_icon;
  primary_icon.type = base::ASCIIToUTF16("image/png");
  primary_icon.sizes.push_back(gfx::Size(144, 144));
  primary_icon.purpose.push_back(content::Manifest::Icon::IconPurpose::ANY);
  primary_icon.src = GURL(kDefaultIconUrl);
  manifest.icons.push_back(primary_icon);

  return manifest;
}

}  // anonymous namespace

class TestInstallableManager : public InstallableManager {
 public:
  explicit TestInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}

  void GetData(const InstallableParams& params,
               const InstallableCallback& callback) override {
    if (should_manifest_time_out_ ||
        (params.check_installable && should_installable_time_out_)) {
      return;
    }

    InstallableStatusCode code = NO_ERROR_DETECTED;
    bool is_installable = is_installable_;
    if (params.fetch_valid_primary_icon && !primary_icon_) {
      code = NO_ACCEPTABLE_ICON;
      is_installable = false;
    } else if (params.check_installable) {
      if (!IsManifestValidForWebApp(manifest_)) {
        code = valid_manifest_->error;
        is_installable = false;
      } else if (!is_installable_) {
        code = NOT_OFFLINE_CAPABLE;
        is_installable = false;
      }
    }

    callback.Run(
        {code, GURL(kDefaultManifestUrl), manifest_,
         params.fetch_valid_primary_icon ? primary_icon_url_ : GURL(),
         params.fetch_valid_primary_icon ? primary_icon_.get() : nullptr,
         params.fetch_valid_badge_icon ? badge_icon_url_ : GURL(),
         params.fetch_valid_badge_icon ? badge_icon_.get() : nullptr,
         params.check_installable ? is_installable : false});
  }

  void SetInstallable(bool is_installable) { is_installable_ = is_installable; }

  void SetManifest(const content::Manifest& manifest) {
    manifest_ = manifest;

    if (!manifest.icons.empty()) {
      primary_icon_url_ = manifest_.icons[0].src;
      primary_icon_.reset(
          new SkBitmap(gfx::test::CreateBitmap(kIconSizePx, kIconSizePx)));

      badge_icon_url_ = manifest_.icons[0].src;
      badge_icon_.reset(
          new SkBitmap(gfx::test::CreateBitmap(kIconSizePx, kIconSizePx)));
    }
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

  void SetShouldInstallableTimeOut(bool should_time_out) {
    should_installable_time_out_ = should_time_out;
  }

 private:
  content::Manifest manifest_;
  GURL primary_icon_url_;
  GURL badge_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  std::unique_ptr<SkBitmap> badge_icon_;

  bool is_installable_ = true;

  bool should_manifest_time_out_ = false;
  bool should_installable_time_out_ = false;
};

// Tests AddToHomescreenDataFetcher. These tests should be browser tests but
// Android does not support browser tests yet (crbug.com/611756).
class AddToHomescreenDataFetcherTest : public ChromeRenderViewHostTestHarness {
 public:
  AddToHomescreenDataFetcherTest() {}
  ~AddToHomescreenDataFetcherTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(profile()->CreateHistoryService(false, true));
    profile()->CreateFaviconService();

    // Manually inject the TestInstallableManager as a "InstallableManager"
    // WebContentsUserData. We can't directly call ::CreateForWebContents due to
    // typing issues since TestInstallableManager doesn't directly inherit from
    // WebContentsUserData.
    web_contents()->SetUserData(
        TestInstallableManager::UserDataKey(),
        base::WrapUnique(new TestInstallableManager(web_contents())));
    installable_manager_ = static_cast<TestInstallableManager*>(
        web_contents()->GetUserData(TestInstallableManager::UserDataKey()));

    NavigateAndCommit(GURL(kDefaultStartUrl));
  }

  std::unique_ptr<AddToHomescreenDataFetcher> BuildFetcher(
      bool check_webapk_compatible,
      AddToHomescreenDataFetcher::Observer* observer) {
    return base::MakeUnique<AddToHomescreenDataFetcher>(
        web_contents(), 1, 1, 1, 1, 1, 500, check_webapk_compatible, observer);
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const char* expected_user_title,
                  const char* expected_name,
                  blink::WebDisplayMode display_mode,
                  bool is_webapk_compatible) {
    WebApplicationInfo web_application_info;
    web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

    fetcher->OnDidGetWebApplicationInfo(web_application_info);
    waiter.WaitForDataAvailable();

    EXPECT_EQ(check_webapk_compatibility(),
              waiter.determined_webapk_compatibility());
    EXPECT_EQ(is_webapk_compatible, waiter.is_webapk_compatible());
    EXPECT_TRUE(waiter.title_available());
    if (is_webapk_compatible)
      EXPECT_TRUE(base::EqualsASCII(waiter.title(), expected_name));
    else
      EXPECT_TRUE(base::EqualsASCII(waiter.title(), expected_user_title));

    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().user_title,
                                  expected_user_title));
    EXPECT_EQ(display_mode, fetcher->shortcut_info().display);
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const char* expected_title,
                  blink::WebDisplayMode display_mode,
                  bool is_webapk_compatible) {
    RunFetcher(fetcher, waiter, expected_title, expected_title, display_mode,
               is_webapk_compatible);
  }

  void SetManifest(const content::Manifest& manifest) {
    installable_manager_->SetManifest(manifest);
  }

  void SetInstallable(bool is_installable) {
    installable_manager_->SetInstallable(is_installable);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    installable_manager_->SetShouldManifestTimeOut(should_time_out);
  }

  void SetShouldInstallableTimeOut(bool should_time_out) {
    installable_manager_->SetShouldInstallableTimeOut(should_time_out);
  }

  virtual bool check_webapk_compatibility() { return true; }

 private:
  TestInstallableManager* installable_manager_;

  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenDataFetcherTest);
};

// Class for tests which should be run with AddToHomescreenDataFetcher built
// with both true and false values of |check_webapk_compatible|.
class AddToHomescreenDataFetcherTestCommon
    : public AddToHomescreenDataFetcherTest,
      public testing::WithParamInterface<bool> {
 public:
  AddToHomescreenDataFetcherTestCommon() {}
  ~AddToHomescreenDataFetcherTestCommon() override {}

  std::unique_ptr<AddToHomescreenDataFetcher> BuildFetcher(
      AddToHomescreenDataFetcher::Observer* observer) {
    return AddToHomescreenDataFetcherTest::BuildFetcher(
        check_webapk_compatibility(), observer);
  }

  // The value of |check_webapk_compatible| used when building the
  // AddToHomescreenDataFetcher.
  bool check_webapk_compatibility() override { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenDataFetcherTestCommon);
};

TEST_P(AddToHomescreenDataFetcherTestCommon, EmptyManifest) {
  // Check that an empty manifest has the appropriate methods run.
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::kWebDisplayModeBrowser, false);
}

TEST_P(AddToHomescreenDataFetcherTestCommon, NoIconManifest) {
  // Test a manifest with no icons. This should use the short name and have
  // a generated icon (empty icon url).
  content::Manifest manifest = BuildDefaultManifest();
  manifest.icons.clear();
  SetManifest(manifest);

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::kWebDisplayModeStandalone, false);

  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
  EXPECT_TRUE(fetcher->badge_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_badge_icon_url.is_empty());
}

TEST_P(AddToHomescreenDataFetcherTestCommon, ManifestFetchTimesOut) {
  // Check that the AddToHomescreenDataFetcher::Observer methods are called
  // if the first call to InstallableManager::GetData() times out. This should
  // fall back to the metadata title and have a non-empty icon (taken from the
  // favicon).
  SetShouldManifestTimeOut(true);
  SetManifest(BuildDefaultManifest());

  // Check a site with no offline-capable service worker.
  SetInstallable(false);
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::kWebDisplayModeBrowser, false);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOut) {
  // Check that the AddToHomescreenDataFetcher::Observer methods are called if
  // the service worker check times out on a page that is installable (i.e. it's
  // taken too long). This should use the short_name and icon from the manifest,
  // but not be WebAPK-compatible. Only relevant when checking WebAPK
  // compatibility.
  SetManifest(BuildDefaultManifest());
  SetShouldInstallableTimeOut(true);

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher =
      BuildFetcher(true, &waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::kWebDisplayModeStandalone, false);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_P(AddToHomescreenDataFetcherTestCommon, InstallableManifest) {
  // Test a site that has an offline-capable service worker.
  content::Manifest manifest(BuildDefaultManifest());
  SetManifest(manifest);

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::kWebDisplayModeStandalone,
             check_webapk_compatibility());

  // There should always be a primary icon.
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));

  // Check that the badge icon is requested only when AddToHomescreenDataFetcher
  // checks for WebAPK compatibility.
  if (check_webapk_compatibility()) {
    EXPECT_FALSE(fetcher->badge_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_badge_icon_url,
              GURL(kDefaultIconUrl));
  } else {
    EXPECT_TRUE(fetcher->badge_icon().drawsNothing());
    EXPECT_TRUE(fetcher->shortcut_info().best_badge_icon_url.is_empty());
  }
}

TEST_P(AddToHomescreenDataFetcherTestCommon,
       ManifestNameClobbersWebApplicationName) {
  // Test that when the manifest provides Manifest::name but not
  // Manifest::short_name that Manifest::name is used as the title.
  {
    // Check the case where we have no icons.
    content::Manifest manifest = BuildDefaultManifest();
    manifest.icons.clear();
    manifest.short_name = base::NullableString16();
    SetManifest(manifest);

    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::kWebDisplayModeStandalone, false);

    EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  content::Manifest manifest(BuildDefaultManifest());
  manifest.short_name = base::NullableString16();
  SetManifest(manifest);

  {
    // Check a site with no offline-capable service worker.
    SetInstallable(false);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::kWebDisplayModeStandalone, false);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  {
    // Check a site where we time out waiting for the service worker.
    SetShouldInstallableTimeOut(true);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::kWebDisplayModeStandalone, false);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  {
    // Check a site with an offline-capaable service worker.
    SetInstallable(true);
    SetShouldInstallableTimeOut(false);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::kWebDisplayModeStandalone, check_webapk_compatibility());

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }
}

TEST_P(AddToHomescreenDataFetcherTestCommon, ManifestNoNameNoShortName) {
  // Test that when the manifest does not provide either Manifest::short_name
  // nor Manifest::name that:
  //  - The page is not WebAPK compatible.
  //  - WebApplicationInfo::title is used as the "name".
  //  - We still use the icons from the manifest.
  content::Manifest manifest(BuildDefaultManifest());
  manifest.name = base::NullableString16();
  manifest.short_name = base::NullableString16();

  // Check the case where we don't time out waiting for the service worker.
  SetManifest(manifest);
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::kWebDisplayModeStandalone, false);

  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().name,
                                kWebApplicationInfoTitle));
  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                kWebApplicationInfoTitle));
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

INSTANTIATE_TEST_CASE_P(CheckWebApkCompatibility,
                        AddToHomescreenDataFetcherTestCommon,
                        ::testing::Values(false, true));
