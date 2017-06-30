// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace {

const char* kWebApplicationInfoTitle = "Meta Title";
const char* kDefaultManifestUrl = "https://www.example.com/manifest.json";
const char* kDefaultManifestName = "Default Name";
const char* kDefaultManifestShortName = "Default Short Name";
const char* kDefaultStartUrl = "https://www.example.com/index.html";
const blink::WebDisplayMode kDefaultManifestDisplayMode =
    blink::kWebDisplayModeStandalone;

// WebContents subclass which mocks out image and manifest fetching.
class MockWebContents : public content::TestWebContents {
 public:
  explicit MockWebContents(content::BrowserContext* browser_context)
      : content::TestWebContents(browser_context),
        should_image_time_out_(false),
        should_manifest_time_out_(false) {}

  ~MockWebContents() override {}

  void SetManifest(const GURL& manifest_url,
                   const content::Manifest& manifest) {
    manifest_url_ = manifest_url;
    manifest_ = manifest;
  }

  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    const ImageDownloadCallback& callback) override {
    if (should_image_time_out_)
      return 0;

    const int kIconSizePx = 144;
    SkBitmap icon = gfx::test::CreateBitmap(kIconSizePx, kIconSizePx);
    std::vector<SkBitmap> icons(1u, icon);
    std::vector<gfx::Size> pixel_sizes(1u, gfx::Size(kIconSizePx, kIconSizePx));
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostTask(FROM_HERE, base::Bind(callback, 0, net::HTTP_OK, url, icons,
                                         pixel_sizes));
    return 0;
  }

  void GetManifest(const GetManifestCallback& callback) override {
    if (should_manifest_time_out_)
      return;

    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostTask(FROM_HERE, base::Bind(callback, manifest_url_, manifest_));
  }

  void SetShouldImageTimeOut(bool should_time_out) {
    should_image_time_out_ = should_time_out;
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

 private:
  GURL manifest_url_;
  content::Manifest manifest_;
  bool should_image_time_out_;
  bool should_manifest_time_out_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContents);
};

// Tracks which of the AddToHomescreenDataFetcher::Observer callbacks have been
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
    EXPECT_FALSE(title_available_);
    determined_webapk_compatibility_ = true;
    is_webapk_compatible_ = is_webapk_compatible;
  }

  void OnUserTitleAvailable(const base::string16& title) override {
    EXPECT_FALSE(data_available_);
    title_available_ = true;
    title_ = title;
  }

  SkBitmap FinalizeLauncherIconInBackground(const SkBitmap& icon,
                                            const GURL& url,
                                            bool* is_generated) override {
    *is_generated = false;
    return icon;
  }

  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& primary_icon,
                       const SkBitmap& badge_icon) override {
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

content::Manifest BuildEmptyManifest() {
  return content::Manifest();
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
  primary_icon.src = GURL("https://www.google.com/image.png");
  manifest.icons.push_back(primary_icon);

  return manifest;
}

}  // anonymous namespace

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

    embedded_worker_test_helper_.reset(
        new content::EmbeddedWorkerTestHelper(base::FilePath()));

    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::Create(browser_context());
    site_instance->GetProcess()->Init();
    MockWebContents* mock_web_contents = new MockWebContents(browser_context());
    mock_web_contents->Init(content::WebContents::CreateParams(
        browser_context(), std::move(site_instance)));
    InstallableManager::CreateForWebContents(mock_web_contents);
    SetContents(mock_web_contents);
    NavigateAndCommit(GURL(kDefaultStartUrl));
  }

  void TearDown() override {
    embedded_worker_test_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  scoped_refptr<AddToHomescreenDataFetcher> BuildFetcher(
      bool check_webapk_compatible,
      AddToHomescreenDataFetcher::Observer* observer) {
    return new AddToHomescreenDataFetcher(web_contents(), 1, 1, 1, 1, 1, 500,
                                          check_webapk_compatible, observer);
  }

  // Set the manifest to be returned as a result of WebContents::GetManifest().
  void SetManifest(const GURL& manifest_url,
                   const content::Manifest& manifest) {
    MockWebContents* mock_web_contents =
        static_cast<MockWebContents*>(web_contents());
    mock_web_contents->SetManifest(manifest_url, manifest);
  }

  void SetShouldImageTimeOut(bool should_time_out) {
    MockWebContents* mock_web_contents =
        static_cast<MockWebContents*>(web_contents());
    mock_web_contents->SetShouldImageTimeOut(should_time_out);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    MockWebContents* mock_web_contents =
        static_cast<MockWebContents*>(web_contents());
    mock_web_contents->SetShouldManifestTimeOut(should_time_out);
  }

  // Registers service worker at |url|. Blocks till the service worker is
  // registered.
  void RegisterServiceWorker(const GURL& url) {
    base::RunLoop run_loop;
    embedded_worker_test_helper_->context()->RegisterServiceWorker(
        url, GURL(url.spec() + "/service_worker.js"), nullptr,
        base::Bind(&AddToHomescreenDataFetcherTest::OnServiceWorkerRegistered,
                   base::Unretained(this), run_loop.QuitClosure()));
  }

 private:
  // Callback for RegisterServiceWorker() for when service worker registration
  // has completed.
  void OnServiceWorkerRegistered(const base::Closure& callback,
                                 content::ServiceWorkerStatusCode status,
                                 const std::string& status_message,
                                 int64_t registration_id) {
    ASSERT_EQ(content::SERVICE_WORKER_OK, status)
        << content::ServiceWorkerStatusToString(status);
    callback.Run();
  }

  std::unique_ptr<content::EmbeddedWorkerTestHelper>
      embedded_worker_test_helper_;

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

  scoped_refptr<AddToHomescreenDataFetcher> BuildFetcher(
      AddToHomescreenDataFetcher::Observer* observer) {
    return AddToHomescreenDataFetcherTest::BuildFetcher(
        check_webapk_compatibility(), observer);
  }

  // The value of |check_webapk_compatible| used when building the
  // AddToHomescreenDataFetcher.
  bool check_webapk_compatibility() { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenDataFetcherTestCommon);
};

// Checks that AddToHomescreenDataFetcher::Observer::OnUserTitleAvailable() is
// called when the web manifest returned is empty. The add-to-homescreen dialog
// makes the dialog's text field editable once OnUserTitleAvailable() is called.
TEST_P(AddToHomescreenDataFetcherTestCommon, EmptyManifest) {
  WebApplicationInfo web_application_info;
  web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

  SetManifest(GURL(kDefaultManifestUrl), BuildEmptyManifest());

  ObserverWaiter waiter;
  scoped_refptr<AddToHomescreenDataFetcher> fetcher(BuildFetcher(&waiter));
  fetcher->OnDidGetWebApplicationInfo(web_application_info);
  waiter.WaitForDataAvailable();

  EXPECT_EQ(check_webapk_compatibility(),
            waiter.determined_webapk_compatibility());
  EXPECT_FALSE(waiter.is_webapk_compatible());
  EXPECT_TRUE(waiter.title_available());
  EXPECT_TRUE(base::EqualsASCII(waiter.title(), kWebApplicationInfoTitle));

  fetcher->set_weak_observer(nullptr);
}

// Test that when the manifest provides Manifest::short_name but not
// Manifest::name that Manifest::short_name is used as the name instead of
// WebApplicationInfo::title.
TEST_P(AddToHomescreenDataFetcherTestCommon,
       ManifestShortNameClobbersWebApplicationName) {
  WebApplicationInfo web_application_info;
  web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

  content::Manifest manifest(BuildDefaultManifest());
  manifest.name = base::NullableString16();

  RegisterServiceWorker(GURL(kDefaultStartUrl));
  SetManifest(GURL(kDefaultManifestUrl), manifest);

  ObserverWaiter waiter;
  scoped_refptr<AddToHomescreenDataFetcher> fetcher(BuildFetcher(&waiter));
  fetcher->OnDidGetWebApplicationInfo(web_application_info);
  waiter.WaitForDataAvailable();

  EXPECT_TRUE(base::EqualsASCII(waiter.title(), kDefaultManifestShortName));
  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().name,
                                kDefaultManifestShortName));

  fetcher->set_weak_observer(nullptr);
}

// Test that when the manifest does not provide either Manifest::short_name nor
// Manifest::name that:
// - The page is not WebAPK compatible.
// - WebApplicationInfo::title is used as the "name".
TEST_P(AddToHomescreenDataFetcherTestCommon, ManifestNoNameNoShortName) {
  WebApplicationInfo web_application_info;
  web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

  content::Manifest manifest(BuildDefaultManifest());
  manifest.name = base::NullableString16();
  manifest.short_name = base::NullableString16();

  RegisterServiceWorker(GURL(kDefaultStartUrl));
  SetManifest(GURL(kDefaultManifestUrl), manifest);

  ObserverWaiter waiter;
  scoped_refptr<AddToHomescreenDataFetcher> fetcher(BuildFetcher(&waiter));
  fetcher->OnDidGetWebApplicationInfo(web_application_info);
  waiter.WaitForDataAvailable();

  EXPECT_EQ(check_webapk_compatibility(),
            waiter.determined_webapk_compatibility());
  EXPECT_FALSE(waiter.is_webapk_compatible());
  EXPECT_TRUE(base::EqualsASCII(waiter.title(), kWebApplicationInfoTitle));
  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().name,
                                kWebApplicationInfoTitle));

  fetcher->set_weak_observer(nullptr);
}

// Checks that the AddToHomescreenDataFetcher::Observer callbacks are called
// when the manifest fetch times out.
TEST_P(AddToHomescreenDataFetcherTestCommon, ManifestFetchTimesOut) {
  WebApplicationInfo web_application_info;
  web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

  RegisterServiceWorker(GURL(kDefaultStartUrl));
  SetManifest(GURL(kDefaultManifestUrl), BuildDefaultManifest());
  SetShouldManifestTimeOut(true);
  SetShouldImageTimeOut(false);

  ObserverWaiter waiter;
  scoped_refptr<AddToHomescreenDataFetcher> fetcher(BuildFetcher(&waiter));
  fetcher->OnDidGetWebApplicationInfo(web_application_info);
  waiter.WaitForDataAvailable();

  EXPECT_EQ(check_webapk_compatibility(),
            waiter.determined_webapk_compatibility());
  EXPECT_FALSE(waiter.is_webapk_compatible());
  EXPECT_TRUE(base::EqualsASCII(waiter.title(), kWebApplicationInfoTitle));
  EXPECT_TRUE(waiter.title_available());

  fetcher->set_weak_observer(nullptr);
}

// Checks that the AddToHomescreenDataFetcher::Observer callbacks are called
// when the image fetch times out.
TEST_P(AddToHomescreenDataFetcherTestCommon, ImageFetchTimesOut) {
  WebApplicationInfo web_application_info;
  web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

  RegisterServiceWorker(GURL(kDefaultStartUrl));
  SetManifest(GURL(kDefaultManifestUrl), BuildDefaultManifest());
  SetShouldManifestTimeOut(false);
  SetShouldImageTimeOut(true);

  ObserverWaiter waiter;
  scoped_refptr<AddToHomescreenDataFetcher> fetcher(BuildFetcher(&waiter));
  fetcher->OnDidGetWebApplicationInfo(web_application_info);
  waiter.WaitForDataAvailable();

  EXPECT_EQ(check_webapk_compatibility(),
            waiter.determined_webapk_compatibility());
  EXPECT_FALSE(waiter.is_webapk_compatible());
  EXPECT_TRUE(waiter.title_available());
  EXPECT_TRUE(base::EqualsASCII(waiter.title(), kWebApplicationInfoTitle));

  fetcher->set_weak_observer(nullptr);
}

// Checks that the AddToHomescreenDataFetcher::Observer callbacks are called
// when the service worker check times out.
TEST_P(AddToHomescreenDataFetcherTestCommon, ServiceWorkerCheckTimesOut) {
  WebApplicationInfo web_application_info;
  web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

  // Not registering a service worker means we'll wait and time out for the
  // worker.
  SetManifest(GURL(kDefaultManifestUrl), BuildDefaultManifest());
  SetShouldManifestTimeOut(false);
  SetShouldImageTimeOut(false);

  ObserverWaiter waiter;
  scoped_refptr<AddToHomescreenDataFetcher> fetcher(BuildFetcher(&waiter));
  fetcher->OnDidGetWebApplicationInfo(web_application_info);
  waiter.WaitForDataAvailable();

  EXPECT_EQ(check_webapk_compatibility(),
            waiter.determined_webapk_compatibility());
  EXPECT_FALSE(waiter.is_webapk_compatible());
  EXPECT_TRUE(waiter.title_available());
  EXPECT_TRUE(base::EqualsASCII(waiter.title(), kDefaultManifestShortName));
  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().user_title,
                                kDefaultManifestShortName));

  fetcher->set_weak_observer(nullptr);
}

INSTANTIATE_TEST_CASE_P(CheckWebApkCompatibility,
                        AddToHomescreenDataFetcherTestCommon,
                        ::testing::Values(false, true));
