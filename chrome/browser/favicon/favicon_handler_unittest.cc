// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/favicon/favicon_handler.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

class TestFaviconHandler;

using content::FaviconURL;
using content::NavigationEntry;
using content::WebContents;

namespace {

// Fill the given bmp with valid png data.
void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp->allocPixels();

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    src_data[i * 4 + 0] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 1] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 2] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 3] = static_cast<unsigned char>(i % 255);
  }
}

// Fill the given data buffer with valid png data.
void FillBitmap(int w, int h, std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  FillDataToBitmap(w, h, &bitmap);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, output);
}

void SetFaviconBitmapResult(
    const GURL& icon_url,
    chrome::IconType icon_type,
    bool expired,
    std::vector<chrome::FaviconBitmapResult>* favicon_bitmap_results) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  chrome::FaviconBitmapResult bitmap_result;
  bitmap_result.expired = expired;
  bitmap_result.bitmap_data = data;
  // Use a pixel size other than (0,0) as (0,0) has a special meaning.
  bitmap_result.pixel_size = gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap_result.icon_type = icon_type;
  bitmap_result.icon_url = icon_url;

  favicon_bitmap_results->push_back(bitmap_result);
}

void SetFaviconBitmapResult(
    const GURL& icon_url,
    std::vector<chrome::FaviconBitmapResult>* favicon_bitmap_results) {
  SetFaviconBitmapResult(icon_url, chrome::FAVICON, false /* expired */,
                         favicon_bitmap_results);
}

// This class is used to save the download request for verifying with test case.
// It also will be used to invoke the onDidDownload callback.
class DownloadHandler {
 public:
  explicit DownloadHandler(TestFaviconHandler* favicon_helper)
      : favicon_helper_(favicon_helper),
        failed_(false) {
  }

  virtual ~DownloadHandler() {
  }

  void Reset() {
    download_.reset(NULL);
    failed_ = false;
  }

  void AddDownload(
      int download_id,
      const GURL& image_url,
      int image_size,
      int max_image_size) {
    download_.reset(new Download(
        download_id, image_url, image_size, max_image_size, false));
  }

  void InvokeCallback();

  void set_failed(bool failed) { failed_ = failed; }

  bool HasDownload() const { return download_.get() != NULL; }
  const GURL& GetImageUrl() const { return download_->image_url; }
  void SetImageSize(int size) { download_->image_size = size; }

 private:
  struct Download {
    Download(int id, GURL url, int size, int max_size, bool failed)
        : download_id(id),
          image_url(url),
          image_size(size),
          max_image_size(max_size) {}
    ~Download() {}
    int download_id;
    GURL image_url;
    int image_size;
    int max_image_size;
  };

  TestFaviconHandler* favicon_helper_;
  scoped_ptr<Download> download_;
  bool failed_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHandler);
};

// This class is used to save the history request for verifying with test case.
// It also will be used to simulate the history response.
class HistoryRequestHandler {
 public:
  HistoryRequestHandler(const GURL& page_url,
                        const GURL& icon_url,
                        int icon_type,
                        const FaviconService::FaviconResultsCallback& callback)
      : page_url_(page_url),
        icon_url_(icon_url),
        icon_type_(icon_type),
        callback_(callback) {
  }

  HistoryRequestHandler(const GURL& page_url,
                        const GURL& icon_url,
                        int icon_type,
                        const std::vector<unsigned char>& bitmap_data)
      : page_url_(page_url),
        icon_url_(icon_url),
        icon_type_(icon_type),
        bitmap_data_(bitmap_data) {
  }

  virtual ~HistoryRequestHandler() {}
  void InvokeCallback();

  const GURL page_url_;
  const GURL icon_url_;
  const int icon_type_;
  const std::vector<unsigned char> bitmap_data_;
  std::vector<chrome::FaviconBitmapResult> history_results_;
  FaviconService::FaviconResultsCallback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryRequestHandler);
};

}  // namespace


class TestFaviconHandlerDelegate : public FaviconHandlerDelegate {
 public:
  TestFaviconHandlerDelegate() {
  }

  virtual ~TestFaviconHandlerDelegate() {
  }

  virtual bool IsOffTheRecord() OVERRIDE { return false; }

  virtual NavigationEntry* GetActiveEntry() OVERRIDE {
    ADD_FAILURE() << "TestFaviconHandlerDelegate::GetActiveEntry() "
                  << "should never be called in tests.";
    return NULL;
  }

  virtual int StartDownload(const GURL& url,
                            int max_bitmap_size) OVERRIDE {
    ADD_FAILURE() << "TestFaviconHandlerDelegate::StartDownload() "
                  << "should never be called in tests.";
    return -1;
  }

  virtual void NotifyFaviconUpdated(bool icon_url_changed) OVERRIDE {
    ADD_FAILURE() << "TestFaviconHandlerDelegate::NotifyFaviconUpdated() "
                  << "should never be called in tests.";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFaviconHandlerDelegate);
};

// This class is used to catch the FaviconHandler's download and history
// request, and also provide the methods to access the FaviconHandler
// internals.
class TestFaviconHandler : public FaviconHandler {
 public:
  TestFaviconHandler(const GURL& page_url,
                     Profile* profile,
                     FaviconHandlerDelegate* delegate,
                     Type type)
      : FaviconHandler(profile, delegate, type),
        entry_(NavigationEntry::Create()),
        download_id_(0),
        num_favicon_updates_(0) {
    entry_->SetURL(page_url);
    download_handler_.reset(new DownloadHandler(this));
  }

  virtual ~TestFaviconHandler() {
  }

  HistoryRequestHandler* history_handler() {
    return history_handler_.get();
  }

  // This method will take the ownership of the given handler.
  void set_history_handler(HistoryRequestHandler* handler) {
    history_handler_.reset(handler);
  }

  DownloadHandler* download_handler() {
    return download_handler_.get();
  }

  size_t num_favicon_update_notifications() const {
    return num_favicon_updates_;
  }

  void ResetNumFaviconUpdateNotifications() {
    num_favicon_updates_ = 0;
  }

  // Methods to access favicon internals.
  virtual NavigationEntry* GetEntry() OVERRIDE {
    return entry_.get();
  }

  const std::deque<FaviconURL>& urls() {
    return image_urls_;
  }

  FaviconURL* current_candidate() {
    return FaviconHandler::current_candidate();
  }

 protected:
  virtual void UpdateFaviconMappingAndFetch(
      const GURL& page_url,
      const GURL& icon_url,
      chrome::IconType icon_type,
      const FaviconService::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(page_url, icon_url,
                                                     icon_type, callback));
  }

  virtual void GetFaviconFromFaviconService(
      const GURL& icon_url,
      chrome::IconType icon_type,
      const FaviconService::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(GURL(), icon_url,
                                                     icon_type, callback));
  }

  virtual void GetFaviconForURLFromFaviconService(
      const GURL& page_url,
      int icon_types,
      const FaviconService::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(page_url, GURL(),
                                                     icon_types, callback));
  }

  virtual int DownloadFavicon(const GURL& image_url,
                              int max_bitmap_size) OVERRIDE {
    download_id_++;
    download_handler_->AddDownload(
        download_id_, image_url, 0, max_bitmap_size);
    return download_id_;
  }

  virtual void SetHistoryFavicons(const GURL& page_url,
                                  const GURL& icon_url,
                                  chrome::IconType icon_type,
                                  const gfx::Image& image) OVERRIDE {
    scoped_refptr<base::RefCountedMemory> bytes = image.As1xPNGBytes();
    std::vector<unsigned char> bitmap_data(bytes->front(),
                                           bytes->front() + bytes->size());
    history_handler_.reset(new HistoryRequestHandler(
        page_url, icon_url, icon_type, bitmap_data));
  }

  virtual FaviconService* GetFaviconService() OVERRIDE {
    // Just give none NULL value, so overridden methods can be hit.
    return (FaviconService*)(1);
  }

  virtual bool ShouldSaveFavicon(const GURL& url) OVERRIDE {
    return true;
  }

  virtual void NotifyFaviconUpdated(bool icon_url_changed) OVERRIDE {
    ++num_favicon_updates_;
  }

  GURL page_url_;

 private:
  scoped_ptr<NavigationEntry> entry_;

  // The unique id of a download request. It will be returned to a
  // FaviconHandler.
  int download_id_;

  scoped_ptr<DownloadHandler> download_handler_;
  scoped_ptr<HistoryRequestHandler> history_handler_;

  // The number of times that NotifyFaviconUpdated() has been called.
  size_t num_favicon_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestFaviconHandler);
};

namespace {

void HistoryRequestHandler::InvokeCallback() {
  if (!callback_.is_null()) {
    callback_.Run(history_results_);
  }
}

void DownloadHandler::InvokeCallback() {
  int original_size = (download_->image_size > 0) ?
      download_->image_size : gfx::kFaviconSize;
  int downloaded_size = original_size;
  if (download_->max_image_size != 0 &&
      downloaded_size > download_->max_image_size) {
    downloaded_size = download_->max_image_size;
  }
  std::vector<SkBitmap> bitmaps;
  std::vector<gfx::Size> original_bitmap_sizes;
  if (!failed_) {
    SkBitmap bitmap;
    FillDataToBitmap(downloaded_size, downloaded_size, &bitmap);
    bitmaps.push_back(bitmap);
    original_bitmap_sizes.push_back(gfx::Size(original_size, original_size));
  }
  favicon_helper_->OnDidDownloadFavicon(download_->download_id,
                                        download_->image_url,
                                        bitmaps,
                                        original_bitmap_sizes);
}

class FaviconHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  FaviconHandlerTest() {
  }

  virtual ~FaviconHandlerTest() {
  }

  // Simulates requesting a favicon for |page_url| given:
  // - We have not previously cached anything in history for |page_url| or for
  //   any of |candidates|.
  // - The page provides favicons at |candidate_icons|.
  // - The favicons at |candidate_icons| have edge pixel sizes of
  //   |candidate_icon_sizes|.
  void DownloadTillDoneIgnoringHistory(
      TestFaviconHandler* favicon_handler,
      const GURL& page_url,
      const std::vector<FaviconURL>& candidate_icons,
      const int* candidate_icon_sizes) {
    favicon_handler->ResetNumFaviconUpdateNotifications();

    favicon_handler->FetchFavicon(page_url);
    favicon_handler->history_handler()->InvokeCallback();

    favicon_handler->OnUpdateFaviconURL(0, candidate_icons);
    EXPECT_EQ(candidate_icons.size(), favicon_handler->image_urls().size());

    DownloadHandler* download_handler = favicon_handler->download_handler();
    for (size_t i = 0; i < candidate_icons.size(); ++i) {
      favicon_handler->history_handler()->history_results_.clear();
      favicon_handler->history_handler()->InvokeCallback();
      ASSERT_TRUE(download_handler->HasDownload());
      EXPECT_EQ(download_handler->GetImageUrl(),
                candidate_icons[i].icon_url);
      download_handler->SetImageSize(candidate_icon_sizes[i]);
      download_handler->InvokeCallback();

      if (favicon_handler->num_favicon_update_notifications())
        return;
    }
  }

  virtual void SetUp() {
    // The score computed by SelectFaviconFrames() is dependent on the supported
    // scale factors of the platform. It is used for determining the goodness of
    // a downloaded bitmap in FaviconHandler::OnDidDownloadFavicon().
    // Force the values of the scale factors so that the tests produce the same
    // results on all platforms.
    std::vector<ui::ScaleFactor> scale_factors;
    scale_factors.push_back(ui::SCALE_FACTOR_100P);
    scoped_set_supported_scale_factors_.reset(
        new ui::test::ScopedSetSupportedScaleFactors(scale_factors));

    ChromeRenderViewHostTestHarness::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    Profile* profile = Profile::FromBrowserContext(
        web_contents()->GetBrowserContext());
    FaviconServiceFactory::GetInstance()->SetTestingFactory(
      profile, NULL);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  typedef scoped_ptr<ui::test::ScopedSetSupportedScaleFactors>
      ScopedSetSupportedScaleFactors;
  ScopedSetSupportedScaleFactors scoped_set_supported_scale_factors_;
  DISALLOW_COPY_AND_ASSIGN(FaviconHandlerTest);
};

TEST_F(FaviconHandlerTest, GetFaviconFromHistory) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::FAVICON, history_handler->icon_type_);

  SetFaviconBitmapResult(icon_url, &history_handler->history_results_);

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);

  // Simulates update favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::FAVICON, helper.current_candidate()->icon_type);

  // Favicon shouldn't request to download icon.
  EXPECT_FALSE(helper.download_handler()->HasDownload());
}

TEST_F(FaviconHandlerTest, DownloadFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::FAVICON, history_handler->icon_type_);

  // Set icon data expired
  SetFaviconBitmapResult(icon_url, chrome::FAVICON, true /* expired */,
                         &history_handler->history_results_);
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);

  // Simulates update favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::FAVICON, helper.current_candidate()->icon_type);

  // Favicon should request to download icon now.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is set.
  helper.set_history_handler(NULL);

  // Smulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  content::FaviconStatus favicon_status = helper.GetEntry()->GetFavicon();
  EXPECT_EQ(icon_url, favicon_status.url);
  EXPECT_TRUE(favicon_status.valid);
  EXPECT_FALSE(favicon_status.image.IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, favicon_status.image.Width());
}

TEST_F(FaviconHandlerTest, UpdateAndDownloadFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::FAVICON, history_handler->icon_type_);

  // Set valid icon data.
  SetFaviconBitmapResult(icon_url, &history_handler->history_results_);

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(NULL);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(new_icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::FAVICON, helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->history_results_.clear();
  history_handler->InvokeCallback();

  // Favicon should request to download icon now.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(new_icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is set.
  helper.set_history_handler(NULL);

  // Smulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  content::FaviconStatus favicon_status = helper.GetEntry()->GetFavicon();
  EXPECT_EQ(new_icon_url, favicon_status.url);
  EXPECT_TRUE(favicon_status.valid);
  EXPECT_FALSE(favicon_status.image.IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, favicon_status.image.Width());
}

TEST_F(FaviconHandlerTest, FaviconInHistoryInvalid) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::FAVICON, history_handler->icon_type_);

  // Set non empty but invalid data.
  chrome::FaviconBitmapResult bitmap_result;
  bitmap_result.expired = false;
  // Empty bitmap data is invalid.
  bitmap_result.bitmap_data = new base::RefCountedBytes();
  bitmap_result.pixel_size = gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap_result.icon_type = chrome::FAVICON;
  bitmap_result.icon_url = icon_url;
  history_handler->history_results_.clear();
  history_handler->history_results_.push_back(bitmap_result);

  // Send history response.
  history_handler->InvokeCallback();
  // The NavigationEntry should not be set yet as the history data is invalid.
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(GURL(), helper.GetEntry()->GetFavicon().url);

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(NULL);

  // Simulates update with matching favicon URL.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // A download for the favicon should be requested, and we should not do
  // another history request.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());
  EXPECT_EQ(NULL, helper.history_handler());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Simulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  content::FaviconStatus favicon_status = helper.GetEntry()->GetFavicon();
  EXPECT_EQ(icon_url, favicon_status.url);
  EXPECT_TRUE(favicon_status.valid);
  EXPECT_FALSE(favicon_status.image.IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, favicon_status.image.Width());
}

TEST_F(FaviconHandlerTest, UpdateFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::FAVICON, history_handler->icon_type_);

  SetFaviconBitmapResult(icon_url, &history_handler->history_results_);

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(NULL);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(new_icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::FAVICON, helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate find icon.
  SetFaviconBitmapResult(new_icon_url, &history_handler->history_results_);
  history_handler->InvokeCallback();

  // Shouldn't request download favicon
  EXPECT_FALSE(helper.download_handler()->HasDownload());

  // Verify the favicon status.
  EXPECT_EQ(new_icon_url, helper.GetEntry()->GetFavicon().url);
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().image.IsEmpty());
}

TEST_F(FaviconHandlerTest, Download2ndFaviconURLCandidate) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::TOUCH);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::TOUCH_PRECOMPOSED_ICON | chrome::TOUCH_ICON,
            history_handler->icon_type_);

  // Icon not found.
  history_handler->history_results_.clear();
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(GURL(), helper.GetEntry()->GetFavicon().url);

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(NULL);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(
      icon_url, FaviconURL::TOUCH_PRECOMPOSED_ICON, std::vector<gfx::Size>()));
  urls.push_back(FaviconURL(
      new_icon_url, FaviconURL::TOUCH_ICON, std::vector<gfx::Size>()));
  urls.push_back(
      FaviconURL(new_icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(2U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::TOUCH_PRECOMPOSED_ICON,
            helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::TOUCH_PRECOMPOSED_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->history_results_.clear();
  history_handler->InvokeCallback();

  // Should request download favicon.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is request from
  // history.
  helper.set_history_handler(NULL);
  // Smulates download failed.
  download_handler->set_failed(true);
  download_handler->InvokeCallback();

  // Left 1 url.
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  EXPECT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Reset download handler
  download_handler->Reset();

  // Simulates getting a expired icon from history.
  SetFaviconBitmapResult(new_icon_url, chrome::TOUCH_ICON,
      true /* expired */, &history_handler->history_results_);
  history_handler->InvokeCallback();

  // Verify the download request.
  EXPECT_TRUE(helper.download_handler()->HasDownload());
  EXPECT_EQ(new_icon_url, download_handler->GetImageUrl());

  helper.set_history_handler(NULL);

  // Simulates icon being downloaded.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);
}

TEST_F(FaviconHandlerTest, UpdateDuringDownloading) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate;
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::TOUCH);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(chrome::TOUCH_PRECOMPOSED_ICON | chrome::TOUCH_ICON,
            history_handler->icon_type_);

  // Icon not found.
  history_handler->history_results_.clear();
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(GURL(), helper.GetEntry()->GetFavicon().url);

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(NULL);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(
      icon_url, FaviconURL::TOUCH_PRECOMPOSED_ICON, std::vector<gfx::Size>()));
  urls.push_back(FaviconURL(
      new_icon_url, FaviconURL::TOUCH_ICON, std::vector<gfx::Size>()));
  urls.push_back(
      FaviconURL(new_icon_url, FaviconURL::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(2U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::TOUCH_PRECOMPOSED_ICON,
            helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::TOUCH_PRECOMPOSED_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->history_results_.clear();
  history_handler->InvokeCallback();

  // Should request download favicon.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is request from
  // history.
  helper.set_history_handler(NULL);
  const GURL latest_icon_url("http://www.google.com/latest_favicon");
  std::vector<FaviconURL> latest_urls;
  latest_urls.push_back(FaviconURL(
      latest_icon_url, FaviconURL::TOUCH_ICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(0, latest_urls);

  EXPECT_EQ(1U, helper.urls().size());
  EXPECT_EQ(latest_icon_url, helper.current_candidate()->icon_url);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, helper.current_candidate()->icon_type);

  // Whether new icon is requested from history
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(latest_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Reset the history_handler to verify whether favicon is request from
  // history.
  // Save the callback for late use.
  FaviconService::FaviconResultsCallback callback = history_handler->callback_;
  helper.set_history_handler(NULL);

  // Simulates download succeed.
  download_handler->InvokeCallback();
  // The downloaded icon should be thrown away as there is favicon update.
  EXPECT_FALSE(helper.history_handler());

  download_handler->Reset();

  // Simulates getting the icon from history.
  scoped_ptr<HistoryRequestHandler> handler;
  handler.reset(new HistoryRequestHandler(page_url, latest_icon_url,
                                          chrome::TOUCH_ICON, callback));
  SetFaviconBitmapResult(latest_icon_url, chrome::TOUCH_ICON,
      false /* expired */, &handler->history_results_);
  handler->InvokeCallback();

  // No download request.
  EXPECT_FALSE(download_handler->HasDownload());
}

#if !defined(OS_ANDROID)

// Test the favicon which is selected when the web page provides several
// favicons and none of the favicons are cached in history.
// The goal of this test is to be more of an integration test than
// SelectFaviconFramesTest.*.
TEST_F(FaviconHandlerTest, MultipleFavicons) {
  const GURL kPageURL("http://www.google.com");
  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(GURL("http://www.google.com/a"),
                 FaviconURL::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/b"),
                 FaviconURL::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/c"),
                 FaviconURL::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/d"),
                 FaviconURL::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/e"),
                 FaviconURL::FAVICON,
                 std::vector<gfx::Size>())};

  // Set the supported scale factors to 1x and 2x. This affects the behavior of
  // SelectFaviconFrames().
  std::vector<ui::ScaleFactor> scale_factors;
  scale_factors.push_back(ui::SCALE_FACTOR_100P);
  scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(scale_factors);

  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());

  // 1) Test that if there are several single resolution favicons to choose from
  // that the largest exact match is chosen.
  TestFaviconHandlerDelegate delegate1;
  TestFaviconHandler handler1(kPageURL, profile,
                              &delegate1, FaviconHandler::FAVICON);
  const int kSizes1[] = { 16, 24, 32, 48, 256 };
  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes1));
  DownloadTillDoneIgnoringHistory(&handler1, kPageURL, urls1, kSizes1);

  content::FaviconStatus favicon_status1(handler1.GetEntry()->GetFavicon());
  EXPECT_EQ(0u, handler1.image_urls().size());
  EXPECT_TRUE(favicon_status1.valid);
  EXPECT_FALSE(favicon_status1.image.IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, favicon_status1.image.Width());

  size_t expected_index = 2u;
  EXPECT_EQ(32, kSizes1[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url,
            handler1.GetEntry()->GetFavicon().url);

  // 2) Test that if there are several single resolution favicons to choose
  // from, the exact match is preferred even if it results in upsampling.
  TestFaviconHandlerDelegate delegate2;
  TestFaviconHandler handler2(kPageURL, profile,
                              &delegate2, FaviconHandler::FAVICON);
  const int kSizes2[] = { 16, 24, 48, 256 };
  std::vector<FaviconURL> urls2(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes2));
  DownloadTillDoneIgnoringHistory(&handler2, kPageURL, urls2, kSizes2);
  EXPECT_TRUE(handler2.GetEntry()->GetFavicon().valid);
  expected_index = 0u;
  EXPECT_EQ(16, kSizes2[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url,
            handler2.GetEntry()->GetFavicon().url);

  // 3) Test that favicons which need to be upsampled a little or downsampled
  // a little are preferred over huge favicons.
  TestFaviconHandlerDelegate delegate3;
  TestFaviconHandler handler3(kPageURL, profile,
                              &delegate3, FaviconHandler::FAVICON);
  const int kSizes3[] = { 256, 48 };
  std::vector<FaviconURL> urls3(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes3));
  DownloadTillDoneIgnoringHistory(&handler3, kPageURL, urls3, kSizes3);
  EXPECT_TRUE(handler3.GetEntry()->GetFavicon().valid);
  expected_index = 1u;
  EXPECT_EQ(48, kSizes3[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url,
            handler3.GetEntry()->GetFavicon().url);

  TestFaviconHandlerDelegate delegate4;
  TestFaviconHandler handler4(kPageURL, profile,
                              &delegate4, FaviconHandler::FAVICON);
  const int kSizes4[] = { 17, 256 };
  std::vector<FaviconURL> urls4(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes4));
  DownloadTillDoneIgnoringHistory(&handler4, kPageURL, urls4, kSizes4);
  EXPECT_TRUE(handler4.GetEntry()->GetFavicon().valid);
  expected_index = 0u;
  EXPECT_EQ(17, kSizes4[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url,
            handler4.GetEntry()->GetFavicon().url);
}

#endif

static KeyedService* BuildFaviconService(content::BrowserContext* profile) {
  return new FaviconService(static_cast<Profile*>(profile));
}

static KeyedService* BuildHistoryService(content::BrowserContext* profile) {
  return NULL;
}

// Test that Favicon is not requested repeatedly during the same session if
// server returns HTTP 404 status.
TEST_F(FaviconHandlerTest, UnableToDownloadFavicon) {
  const GURL missing_icon_url("http://www.google.com/favicon.ico");
  const GURL another_icon_url("http://www.youtube.com/favicon.ico");

  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());

  FaviconServiceFactory::GetInstance()->SetTestingFactory(
      profile, BuildFaviconService);

  HistoryServiceFactory::GetInstance()->SetTestingFactory(
      profile, BuildHistoryService);

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::IMPLICIT_ACCESS);

  FaviconTabHelper::CreateForWebContents(web_contents());
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents());

  std::vector<SkBitmap> empty_icons;
  std::vector<gfx::Size> empty_icon_sizes;
  int download_id = 0;

  // Try to download missing icon.
  download_id = favicon_tab_helper->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));

  // Report download failure with HTTP 503 status.
  favicon_tab_helper->DidDownloadFavicon(download_id, 503, missing_icon_url,
      empty_icons, empty_icon_sizes);
  // Icon is not marked as UnableToDownload as HTTP status is not 404.
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));

  // Try to download again.
  download_id = favicon_tab_helper->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));

  // Report download failure with HTTP 404 status.
  favicon_tab_helper->DidDownloadFavicon(download_id, 404, missing_icon_url,
      empty_icons, empty_icon_sizes);
  // Icon is marked as UnableToDownload.
  EXPECT_TRUE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));

  // Try to download again.
  download_id = favicon_tab_helper->StartDownload(missing_icon_url, 0);
  // Download is not started and Icon is still marked as UnableToDownload.
  EXPECT_EQ(0, download_id);
  EXPECT_TRUE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));

  // Try to download another icon.
  download_id = favicon_tab_helper->StartDownload(another_icon_url, 0);
  // Download is started as another icon URL is not same as missing_icon_url.
  EXPECT_NE(0, download_id);
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(another_icon_url));

  // Clear the list of missing icons.
  favicon_service->ClearUnableToDownloadFavicons();
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(another_icon_url));

  // Try to download again.
  download_id = favicon_tab_helper->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  // Report download success with HTTP 200 status.
  favicon_tab_helper->DidDownloadFavicon(download_id, 200, missing_icon_url,
      empty_icons, empty_icon_sizes);
  // Icon is not marked as UnableToDownload as HTTP status is not 404.
  EXPECT_FALSE(favicon_service->WasUnableToDownloadFavicon(missing_icon_url));
}

}  // namespace.
