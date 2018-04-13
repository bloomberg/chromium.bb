// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/customization/customization_wallpaper_downloader.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kOEMWallpaperURL[] = "http://somedomain.com/image.png";

constexpr char kServicesManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"default_wallpaper\": \"\%s\",\n"
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

std::string ManifestForURL(const std::string& url) {
  return base::StringPrintf(kServicesManifest, url.c_str());
}

// Expected minimal wallpaper download retry interval in milliseconds.
constexpr int kDownloadRetryIntervalMS = 100;

// Dimension used for width and height of default wallpaper images. A small
// value is used to minimize the amount of time spent compressing and writing
// images.
constexpr int kWallpaperSize = 2;

constexpr SkColor kCustomizedDefaultWallpaperColor = SK_ColorDKGRAY;

// Returns true if the color at the center of |image| is close to
// |expected_color|. (The center is used so small wallpaper images can be
// used.)
bool ImageIsNearColor(gfx::ImageSkia image, SkColor expected_color) {
  if (image.size().IsEmpty()) {
    LOG(ERROR) << "Image is empty";
    return false;
  }

  const SkBitmap* bitmap = image.bitmap();
  if (!bitmap) {
    LOG(ERROR) << "Unable to get bitmap from image";
    return false;
  }

  gfx::Point center = gfx::Rect(image.size()).CenterPoint();
  SkColor image_color = bitmap->getColor(center.x(), center.y());

  const int kDiff = 3;
  if (std::abs(static_cast<int>(SkColorGetA(image_color)) -
               static_cast<int>(SkColorGetA(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetR(image_color)) -
               static_cast<int>(SkColorGetR(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetG(image_color)) -
               static_cast<int>(SkColorGetG(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetB(image_color)) -
               static_cast<int>(SkColorGetB(expected_color))) > kDiff) {
    LOG(ERROR) << "Expected color near 0x" << std::hex << expected_color
               << " but got 0x" << image_color;
    return false;
  }

  return true;
}

class TestWallpaperObserver : public ash::mojom::WallpaperObserver {
 public:
  TestWallpaperObserver() : finished_(false), observer_binding_(this) {
    ash::mojom::WallpaperObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    WallpaperControllerClient::Get()->AddObserver(std::move(ptr_info));
  }

  ~TestWallpaperObserver() override = default;

  // ash::mojom::WallpaperObserver:
  void OnWallpaperChanged(uint32_t image_id) override {
    finished_ = true;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  void OnWallpaperColorsChanged(
      const std::vector<SkColor>& prominent_colors) override {}

  void OnWallpaperBlurChanged(bool blurred) override {}

  // Wait until the wallpaper update is completed.
  void WaitForWallpaperChanged() {
    while (!finished_)
      base::RunLoop().Run();
  }

  void Reset() { finished_ = false; }

 private:
  bool finished_;

  // The binding this instance uses to implement ash::mojom::WallpaperObserver.
  mojo::AssociatedBinding<ash::mojom::WallpaperObserver> observer_binding_;

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
  TestWallpaperObserver observer;
  // Show a built-in default wallpaper first.
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Start fetching the customized default wallpaper.
  WallpaperImageFetcherFactory url_factory(
      GURL(kOEMWallpaperURL), kWallpaperSize, kWallpaperSize,
      kCustomizedDefaultWallpaperColor, 0 /* require_retries */);
  chromeos::ServicesCustomizationDocument* customization =
      chromeos::ServicesCustomizationDocument::GetInstance();
  EXPECT_TRUE(
      customization->LoadManifestFromString(ManifestForURL(kOEMWallpaperURL)));
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Verify the customized default wallpaper has replaced the built-in default
  // wallpaper.
  base::RunLoop run_loop;
  WallpaperControllerClient::Get()->GetWallpaperImage(
      base::BindLambdaForTesting([&run_loop](const gfx::ImageSkia& image) {
        run_loop.Quit();
        EXPECT_TRUE(ImageIsNearColor(image, kCustomizedDefaultWallpaperColor));
      }));
  run_loop.Run();
  EXPECT_EQ(1U, url_factory.num_attempts());
}

IN_PROC_BROWSER_TEST_F(CustomizationWallpaperDownloaderBrowserTest,
                       OEMWallpaperRetryFetch) {
  TestWallpaperObserver observer;
  // Show a built-in default wallpaper.
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Start fetching the customized default wallpaper.
  WallpaperImageFetcherFactory url_factory(
      GURL(kOEMWallpaperURL), kWallpaperSize, kWallpaperSize,
      kCustomizedDefaultWallpaperColor, 1 /* require_retries */);
  chromeos::ServicesCustomizationDocument* customization =
      chromeos::ServicesCustomizationDocument::GetInstance();
  EXPECT_TRUE(
      customization->LoadManifestFromString(ManifestForURL(kOEMWallpaperURL)));
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Verify the customized default wallpaper has replaced the built-in default
  // wallpaper.
  base::RunLoop run_loop;
  WallpaperControllerClient::Get()->GetWallpaperImage(
      base::BindLambdaForTesting([&run_loop](const gfx::ImageSkia& image) {
        run_loop.Quit();
        EXPECT_TRUE(ImageIsNearColor(image, kCustomizedDefaultWallpaperColor));
      }));
  run_loop.Run();
  EXPECT_EQ(2U, url_factory.num_attempts());
}

}  // namespace chromeos
