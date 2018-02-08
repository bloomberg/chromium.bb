// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/customization/customization_wallpaper_downloader.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kOEMWallpaperURL[] = "http://somedomain.com/image.png";

const char kServicesManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"default_wallpaper\": \"http://somedomain.com/image.png\",\n"
    "  \"default_apps\": [\n"
    "    \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n"
    "    \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"\n"
    "  ],\n"
    "  \"localized_content\": {\n"
    "    \"en-US\": {\n"
    "      \"default_apps_folder_name\": \"EN-US OEM Name\"\n"
    "    },\n"
    "    \"en\": {\n"
    "      \"default_apps_folder_name\": \"EN OEM Name\"\n"
    "    },\n"
    "    \"default\": {\n"
    "      \"default_apps_folder_name\": \"Default OEM Name\"\n"
    "    }\n"
    "  }\n"
    "}";

// Expected minimal wallpaper download retry interval in milliseconds.
const int kDownloadRetryIntervalMS = 100;

class TestWallpaperObserver : public ash::WallpaperControllerObserver {
 public:
  explicit TestWallpaperObserver(ash::WallpaperController* wallpaper_controller)
      : finished_(false), wallpaper_controller_(wallpaper_controller) {
    wallpaper_controller_->AddObserver(this);
  }

  ~TestWallpaperObserver() override {
    wallpaper_controller_->RemoveObserver(this);
  }

  void OnWallpaperDataChanged() override {
    finished_ = true;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  // Wait until the wallpaper update is completed.
  void WaitForWallpaperDataChanged() {
    while (!finished_)
      base::RunLoop().Run();
  }

  void Reset() { finished_ = false; }

 private:
  bool finished_;
  ash::WallpaperController* wallpaper_controller_;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperObserver);
};

}  // namespace

// This is helper class for net::FakeURLFetcherFactory.
class TestWallpaperImageURLFetcherCallback {
 public:
  TestWallpaperImageURLFetcherCallback(
      const GURL& url,
      const size_t require_retries,
      const std::vector<unsigned char>& jpeg_data_raw)
      : url_(url), require_retries_(require_retries), factory_(nullptr) {
    jpeg_data_.resize(jpeg_data_raw.size());
    std::copy(jpeg_data_raw.begin(), jpeg_data_raw.end(), jpeg_data_.begin());
  }

  std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    chromeos::ServicesCustomizationDocument* customization =
        chromeos::ServicesCustomizationDocument::GetInstance();
    customization->wallpaper_downloader_for_testing()
        ->set_retry_delay_for_testing(
            base::TimeDelta::FromMilliseconds(kDownloadRetryIntervalMS));

    attempts_.push_back(base::TimeTicks::Now());
    if (attempts_.size() > 1) {
      const int retry = num_attempts() - 1;
      const base::TimeDelta current_delay =
          customization->wallpaper_downloader_for_testing()
              ->retry_current_delay_for_testing();
      const double base_interval = base::TimeDelta::FromMilliseconds(
                                       kDownloadRetryIntervalMS).InSecondsF();
      EXPECT_GE(current_delay,
                base::TimeDelta::FromSecondsD(base_interval * retry * retry))
          << "Retry too fast. Actual interval " << current_delay.InSecondsF()
          << " seconds, but expected at least " << base_interval
          << " * (retry=" << retry
          << " * retry)= " << base_interval * retry * retry << " seconds.";
    }
    if (attempts_.size() > require_retries_) {
      response_code = net::HTTP_OK;
      status = net::URLRequestStatus::SUCCESS;
      factory_->SetFakeResponse(url, response_data, response_code, status);
    }
    std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));
    scoped_refptr<net::HttpResponseHeaders> download_headers =
        new net::HttpResponseHeaders(std::string());
    download_headers->AddHeader("Content-Type: image/jpeg");
    fetcher->set_response_headers(download_headers);
    return fetcher;
  }

  void Initialize(net::FakeURLFetcherFactory* factory) {
    factory_ = factory;
    factory_->SetFakeResponse(url_,
                              jpeg_data_,
                              net::HTTP_INTERNAL_SERVER_ERROR,
                              net::URLRequestStatus::FAILED);
  }

  size_t num_attempts() const { return attempts_.size(); }

 private:
  const GURL url_;
  // Respond with OK on required retry attempt.
  const size_t require_retries_;
  net::FakeURLFetcherFactory* factory_;
  std::vector<base::TimeTicks> attempts_;
  std::string jpeg_data_;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperImageURLFetcherCallback);
};

// This implements fake remote source for wallpaper image.
// JPEG image is created here and served to CustomizationWallpaperDownloader
// via net::FakeURLFetcher.
class WallpaperImageFetcherFactory {
 public:
  WallpaperImageFetcherFactory(const GURL& url,
                               int width,
                               int height,
                               SkColor color,
                               const size_t require_retries) {
    // ASSERT_TRUE() cannot be directly used in constructor.
    Initialize(url, width, height, color, require_retries);
  }

  ~WallpaperImageFetcherFactory() {
    fetcher_factory_.reset();
    net::URLFetcherImpl::set_factory(fallback_fetcher_factory_.get());
    fallback_fetcher_factory_.reset();
  }

  size_t num_attempts() const { return url_callback_->num_attempts(); }

 private:
  void Initialize(const GURL& url,
                  int width,
                  int height,
                  SkColor color,
                  const size_t require_retries) {
    std::vector<unsigned char> oem_wallpaper_;
    ASSERT_TRUE(ash::WallpaperController::CreateJPEGImageForTesting(
        width, height, color, &oem_wallpaper_));

    url_callback_.reset(new TestWallpaperImageURLFetcherCallback(
        url, require_retries, oem_wallpaper_));
    fallback_fetcher_factory_.reset(new net::TestURLFetcherFactory);
    net::URLFetcherImpl::set_factory(nullptr);
    fetcher_factory_.reset(new net::FakeURLFetcherFactory(
        fallback_fetcher_factory_.get(),
        base::Bind(&TestWallpaperImageURLFetcherCallback::CreateURLFetcher,
                   base::Unretained(url_callback_.get()))));
    url_callback_->Initialize(fetcher_factory_.get());
  }

  std::unique_ptr<TestWallpaperImageURLFetcherCallback> url_callback_;

  // Use a test factory as a fallback so we don't have to deal with other
  // requests.
  std::unique_ptr<net::TestURLFetcherFactory> fallback_fetcher_factory_;
  std::unique_ptr<net::FakeURLFetcherFactory> fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperImageFetcherFactory);
};

class CustomizationWallpaperDownloaderBrowserTest
    : public InProcessBrowserTest {
 public:
  CustomizationWallpaperDownloaderBrowserTest() {}
  ~CustomizationWallpaperDownloaderBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomizationWallpaperDownloaderBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CustomizationWallpaperDownloaderBrowserTest,
                       OEMWallpaperIsPresent) {
  ash::WallpaperController* wallpaper_controller =
      ash::Shell::Get()->wallpaper_controller();
  TestWallpaperObserver observer(wallpaper_controller);
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  observer.WaitForWallpaperDataChanged();
  observer.Reset();

  WallpaperImageFetcherFactory url_factory(
      GURL(kOEMWallpaperURL), wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kLargeCustomWallpaperColor,
      0 /* require_retries */);

  chromeos::ServicesCustomizationDocument* customization =
      chromeos::ServicesCustomizationDocument::GetInstance();
  EXPECT_TRUE(
      customization->LoadManifestFromString(std::string(kServicesManifest)));

  observer.WaitForWallpaperDataChanged();
  observer.Reset();

  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      wallpaper_controller->GetWallpaper(),
      wallpaper_manager_test_utils::kLargeCustomWallpaperColor));
  EXPECT_EQ(1U, url_factory.num_attempts());
}

IN_PROC_BROWSER_TEST_F(CustomizationWallpaperDownloaderBrowserTest,
                       OEMWallpaperRetryFetch) {
  ash::WallpaperController* wallpaper_controller =
      ash::Shell::Get()->wallpaper_controller();
  TestWallpaperObserver observer(wallpaper_controller);
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  observer.WaitForWallpaperDataChanged();
  observer.Reset();

  WallpaperImageFetcherFactory url_factory(
      GURL(kOEMWallpaperURL), wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kLargeCustomWallpaperColor,
      1 /* require_retries */);

  chromeos::ServicesCustomizationDocument* customization =
      chromeos::ServicesCustomizationDocument::GetInstance();
  EXPECT_TRUE(
      customization->LoadManifestFromString(std::string(kServicesManifest)));

  observer.WaitForWallpaperDataChanged();
  observer.Reset();

  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      wallpaper_controller->GetWallpaper(),
      wallpaper_manager_test_utils::kLargeCustomWallpaperColor));

  EXPECT_EQ(2U, url_factory.num_attempts());
}

}  // namespace chromeos
