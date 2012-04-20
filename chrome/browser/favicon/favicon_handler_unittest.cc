// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/favicon/favicon_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

class TestFaviconHandler;

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

  void AddDownload(int download_id, const GURL& image_url, int image_size) {
    download_.reset(new Download(download_id, image_url, image_size, false));
  }

  void InvokeCallback();

  void set_failed(bool failed) { failed_ = failed; }

  bool HasDownload() const { return download_.get() != NULL; }
  const GURL& GetImageUrl() const { return download_->image_url; }
  int GetImageSize() const { return download_->image_size; }
  void SetImageSize(int size) { download_->image_size = size; }

 private:
  struct Download {
    Download(int id, GURL url, int size, bool failed)
        : download_id(id),
          image_url(url),
          image_size(size) {}
    ~Download() {}
    int download_id;
    GURL image_url;
    int image_size;
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
                        const FaviconService::FaviconDataCallback& callback)
    : page_url_(page_url),
      icon_url_(icon_url),
      icon_type_(icon_type),
      callback_(callback) {
  }

  HistoryRequestHandler(const GURL& page_url,
                        const GURL& icon_url,
                        int icon_type,
                        const std::vector<unsigned char>& image_data,
                        const FaviconService::FaviconDataCallback& callback)
    : page_url_(page_url),
      icon_url_(icon_url),
      icon_type_(icon_type),
      image_data_(image_data),
      callback_(callback) {
  }

  virtual ~HistoryRequestHandler() {}
  void InvokeCallback();

  const GURL page_url_;
  const GURL icon_url_;
  const int icon_type_;
  const std::vector<unsigned char> image_data_;
  history::FaviconData favicon_data_;
  FaviconService::FaviconDataCallback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryRequestHandler);
};

}  // namespace


// This class is used as a temporary hack to provide working implementations of
// the various delegate methods.  Most of these methods are actually never
// called.
// TODO(rohitrao): Refactor the tests to override these delegate methods instead
// of subclassing.
class TestFaviconHandlerDelegate : public FaviconHandlerDelegate {
 public:
  explicit TestFaviconHandlerDelegate(WebContents* tab_contents)
      : tab_contents_(tab_contents) {
  }

  virtual NavigationEntry* GetActiveEntry() {
    ADD_FAILURE() << "TestFaviconHandlerDelegate::GetActiveEntry() "
                  << "should never be called in tests.";
    return NULL;
  }

  virtual void StartDownload(int id, const GURL& url, int image_size) {
    ADD_FAILURE() << "TestFaviconHandlerDelegate::StartDownload() "
                  << "should never be called in tests.";
  }

  virtual void NotifyFaviconUpdated() {
    tab_contents_->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }

 private:
  WebContents* tab_contents_;  // weak
};

// This class is used to catch the FaviconHandler's download and history
// request, and also provide the methods to access the FaviconHandler internal.
class TestFaviconHandler : public FaviconHandler {
 public:
  TestFaviconHandler(const GURL& page_url,
                     Profile* profile,
                     FaviconHandlerDelegate* delegate,
                     Type type)
      : FaviconHandler(profile, delegate, type),
        download_image_size_(0),
        entry_(NavigationEntry::Create()),
        download_id_(0) {
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

  virtual NavigationEntry* GetEntry() {
    return entry_.get();
  }

  const std::deque<FaviconURL>& urls() {
    return image_urls_;
  }

  void FetchFavicon(const GURL& url) {
    FaviconHandler::FetchFavicon(url);
  }

  // The methods to access favicon internal.
  FaviconURL* current_candidate() {
    return FaviconHandler::current_candidate();
  }

 protected:
  virtual void UpdateFaviconMappingAndFetch(
      const GURL& page_url,
      const GURL& icon_url,
      history::IconType icon_type,
      CancelableRequestConsumerBase* consumer,
      const FaviconService::FaviconDataCallback& callback) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(page_url, icon_url,
                                                     icon_type, callback));
  }

  virtual void GetFavicon(
      const GURL& icon_url,
      history::IconType icon_type,
      CancelableRequestConsumerBase* consumer,
      const FaviconService::FaviconDataCallback& callback) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(GURL(), icon_url,
                                                     icon_type, callback));
  }

  virtual void GetFaviconForURL(
      const GURL& page_url,
      int icon_types,
      CancelableRequestConsumerBase* consumer,
      const FaviconService::FaviconDataCallback& callback) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(page_url, GURL(),
                                                     icon_types, callback));
  }

  virtual int DownloadFavicon(const GURL& image_url, int image_size) OVERRIDE {
    download_id_++;
    download_handler_->AddDownload(download_id_, image_url, image_size);
    return download_id_;
  }

  virtual void SetHistoryFavicon(const GURL& page_url,
                                 const GURL& icon_url,
                                 const std::vector<unsigned char>& image_data,
                                 history::IconType icon_type) OVERRIDE {
    history_handler_.reset(new HistoryRequestHandler(
        page_url, icon_url,icon_type, image_data,
        FaviconService::FaviconDataCallback()));
  }

  virtual FaviconService* GetFaviconService() OVERRIDE {
    // Just give none NULL value, so overridden methods can be hit.
    return (FaviconService*)(1);
  }

  virtual bool ShouldSaveFavicon(const GURL& url) OVERRIDE {
    return true;
  }

  GURL page_url_;

  GURL download_image_url_;
  int download_image_size_;

 private:
  scoped_ptr<NavigationEntry> entry_;

  // The unique id of a download request. It will be returned to a
  // FaviconHandler.
  int download_id_;

  scoped_ptr<DownloadHandler> download_handler_;
  scoped_ptr<HistoryRequestHandler> history_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestFaviconHandler);
};

namespace {

void HistoryRequestHandler::InvokeCallback() {
  if (!callback_.is_null())
    callback_.Run(0, favicon_data_);
}

void DownloadHandler::InvokeCallback() {
  SkBitmap bitmap;
  int bitmap_size = (download_->image_size > 0) ?
      download_->image_size : gfx::kFaviconSize;
  FillDataToBitmap(bitmap_size, bitmap_size, &bitmap);
  gfx::Image image(bitmap);
  favicon_helper_->OnDidDownloadFavicon(
      download_->download_id, download_->image_url, failed_, image);
}

class FaviconHandlerTest : public ChromeRenderViewHostTestHarness {
};

TEST_F(FaviconHandlerTest, GetFaviconFromHistory) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(history::FAVICON, history_handler->icon_type_);

  // Set valid icon data.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = false;
  history_handler->favicon_data_.icon_url = icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  history_handler->favicon_data_.image_data = data;

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);

  // Simulates update favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(icon_url, FaviconURL::FAVICON));
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

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(history::FAVICON, history_handler->icon_type_);

  // Set icon data expired
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = true;
  history_handler->favicon_data_.icon_url = icon_url;
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);

  // Simulates update favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(icon_url, FaviconURL::FAVICON));
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
  EXPECT_EQ(gfx::kFaviconSize, download_handler->GetImageSize());

  // Reset the history_handler to verify whether favicon is set.
  helper.set_history_handler(NULL);

  // Smulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->image_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  EXPECT_EQ(icon_url, helper.GetEntry()->GetFavicon().url);
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().bitmap.empty());
}

TEST_F(FaviconHandlerTest, UpdateAndDownloadFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(history::FAVICON, history_handler->icon_type_);

  // Set valid icon data.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = false;
  history_handler->favicon_data_.icon_url = icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  history_handler->favicon_data_.image_data = data;

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
  urls.push_back(FaviconURL(new_icon_url, FaviconURL::FAVICON));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::FAVICON, helper.current_candidate()->icon_type);
  // The favicon status's url should be updated.
  ASSERT_EQ(new_icon_url, helper.GetEntry()->GetFavicon().url);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->favicon_data_.known_icon = false;
  history_handler->InvokeCallback();

  // Favicon should request to download icon now.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(new_icon_url, download_handler->GetImageUrl());
  EXPECT_EQ(gfx::kFaviconSize, download_handler->GetImageSize());

  // Reset the history_handler to verify whether favicon is set.
  helper.set_history_handler(NULL);

  // Smulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->image_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  EXPECT_EQ(new_icon_url, helper.GetEntry()->GetFavicon().url);
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().bitmap.empty());
}

TEST_F(FaviconHandlerTest, UpdateFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::FAVICON);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(history::FAVICON, history_handler->icon_type_);

  // Set valid icon data.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = false;
  history_handler->favicon_data_.icon_url = icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  history_handler->favicon_data_.image_data = data;

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
  urls.push_back(FaviconURL(new_icon_url, FaviconURL::FAVICON));
  helper.OnUpdateFaviconURL(0, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(1U, helper.urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(FaviconURL::FAVICON, helper.current_candidate()->icon_type);
  // The favicon status's url should be updated.
  ASSERT_EQ(new_icon_url, helper.GetEntry()->GetFavicon().url);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::FAVICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate find icon.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = false;
  history_handler->favicon_data_.icon_url = new_icon_url;
  history_handler->favicon_data_.image_data = data;
  history_handler->InvokeCallback();

  // Shouldn't request download favicon
  EXPECT_FALSE(helper.download_handler()->HasDownload());

  // Verify the favicon status.
  EXPECT_EQ(new_icon_url, helper.GetEntry()->GetFavicon().url);
  EXPECT_TRUE(helper.GetEntry()->GetFavicon().valid);
  EXPECT_FALSE(helper.GetEntry()->GetFavicon().bitmap.empty());
}

TEST_F(FaviconHandlerTest, Download2ndFaviconURLCandidate) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::TOUCH);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON,
            history_handler->icon_type_);

  // Icon not found.
  history_handler->favicon_data_.known_icon = false;
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
  urls.push_back(FaviconURL(icon_url, FaviconURL::TOUCH_PRECOMPOSED_ICON));
  urls.push_back(FaviconURL(new_icon_url, FaviconURL::TOUCH_ICON));
  urls.push_back(FaviconURL(new_icon_url, FaviconURL::FAVICON));
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
  history_handler->favicon_data_.known_icon = false;
  history_handler->InvokeCallback();

  // Should request download favicon.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());
  EXPECT_EQ(0, download_handler->GetImageSize());

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

  // Smulates getting a expired icon from history.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::TOUCH_ICON;
  history_handler->favicon_data_.expired = true;
  history_handler->favicon_data_.icon_url = new_icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  history_handler->favicon_data_.image_data = data;
  history_handler->InvokeCallback();

  // Verify the download request.
  EXPECT_TRUE(helper.download_handler()->HasDownload());
  EXPECT_EQ(new_icon_url, download_handler->GetImageUrl());
  EXPECT_EQ(0, download_handler->GetImageSize());

  helper.set_history_handler(NULL);

  // Simulates icon being downloaded.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->image_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);
}

TEST_F(FaviconHandlerTest, UpdateDuringDownloading) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler helper(page_url, profile,
                            &delegate, FaviconHandler::TOUCH);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON,
            history_handler->icon_type_);

  // Icon not found.
  history_handler->favicon_data_.known_icon = false;
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
  urls.push_back(FaviconURL(icon_url, FaviconURL::TOUCH_PRECOMPOSED_ICON));
  urls.push_back(FaviconURL(new_icon_url, FaviconURL::TOUCH_ICON));
  urls.push_back(FaviconURL(new_icon_url, FaviconURL::FAVICON));
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
  history_handler->favicon_data_.known_icon = false;
  history_handler->InvokeCallback();

  // Should request download favicon.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());
  EXPECT_EQ(0, download_handler->GetImageSize());

  // Reset the history_handler to verify whether favicon is request from
  // history.
  helper.set_history_handler(NULL);
  const GURL latest_icon_url("http://www.google.com/latest_favicon");
  std::vector<FaviconURL> latest_urls;
  latest_urls.push_back(FaviconURL(latest_icon_url, FaviconURL::TOUCH_ICON));
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
  FaviconService::FaviconDataCallback callback = history_handler->callback_;
  helper.set_history_handler(NULL);

  // Simulates download succeed.
  download_handler->InvokeCallback();
  // The downloaded icon should be thrown away as there is favicon update.
  EXPECT_FALSE(helper.history_handler());

  download_handler->Reset();

  // Simulates getting the icon from history.
  scoped_ptr<HistoryRequestHandler> handler;
  handler.reset(new HistoryRequestHandler(page_url, latest_icon_url,
                                          history::TOUCH_ICON, callback));
  handler->favicon_data_.known_icon = true;
  handler->favicon_data_.expired = false;
  handler->favicon_data_.icon_type = history::TOUCH_ICON;
  handler->favicon_data_.icon_url = latest_icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  handler->favicon_data_.image_data = data;

  handler->InvokeCallback();

  // No download request.
  EXPECT_FALSE(download_handler->HasDownload());
}

TEST_F(FaviconHandlerTest, MultipleFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL icon_url_small("http://www.google.com/favicon_small");
  const GURL icon_url_large("http://www.google.com/favicon_large");
  const GURL icon_url_preferred1("http://www.google.com/favicon_preferred1");
  const GURL icon_url_preferred2("http://www.google.com/favicon_preferred2");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler handler(page_url, profile,
                             &delegate, FaviconHandler::FAVICON);

  handler.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = handler.history_handler();

  // Set valid icon data.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = false;
  history_handler->favicon_data_.icon_url = icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  history_handler->favicon_data_.image_data = data;

  // Send history response.
  history_handler->InvokeCallback();

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  // Note: the code will stop making requests when an icon matching the
  // preferred size is found, so icon_url_preferred must be last.
  urls.push_back(FaviconURL(icon_url_small, FaviconURL::FAVICON));
  urls.push_back(FaviconURL(icon_url_large, FaviconURL::FAVICON));
  urls.push_back(FaviconURL(icon_url_preferred1, FaviconURL::FAVICON));
  urls.push_back(FaviconURL(icon_url_preferred2, FaviconURL::FAVICON));
  handler.OnUpdateFaviconURL(0, urls);
  EXPECT_EQ(4U, handler.image_urls().size());

  DownloadHandler* download_handler = handler.download_handler();

  // Download the first icon (set not in history).
  handler.history_handler()->favicon_data_.known_icon = false;
  handler.history_handler()->InvokeCallback();
  ASSERT_TRUE(download_handler->HasDownload());
  EXPECT_EQ(icon_url_small, download_handler->GetImageUrl());
  download_handler->SetImageSize(gfx::kFaviconSize / 2);
  download_handler->InvokeCallback();
  EXPECT_EQ(3U, handler.image_urls().size());

  // Download the second icon (set not in history).
  handler.history_handler()->favicon_data_.known_icon = false;
  handler.history_handler()->InvokeCallback();
  ASSERT_TRUE(download_handler->HasDownload());
  EXPECT_EQ(icon_url_large, download_handler->GetImageUrl());
  download_handler->SetImageSize(gfx::kFaviconSize * 2);
  download_handler->InvokeCallback();
  EXPECT_EQ(2U, handler.image_urls().size());

  // Download the third icon (set not in history).
  handler.history_handler()->favicon_data_.known_icon = false;
  handler.history_handler()->InvokeCallback();
  ASSERT_TRUE(download_handler->HasDownload());
  EXPECT_EQ(icon_url_preferred1, download_handler->GetImageUrl());
  download_handler->SetImageSize(gfx::kFaviconSize);
  download_handler->InvokeCallback();
  // Verify that this was detected as an exact match and image_urls_ is cleared.
  EXPECT_EQ(0U, handler.image_urls().size());

  // Verify correct icon size chosen.
  EXPECT_EQ(icon_url_preferred1, handler.GetEntry()->GetFavicon().url);
  EXPECT_TRUE(handler.GetEntry()->GetFavicon().valid);
  EXPECT_FALSE(handler.GetEntry()->GetFavicon().bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, handler.GetEntry()->GetFavicon().bitmap.width());
}

TEST_F(FaviconHandlerTest, FirstFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL icon_url_preferred1("http://www.google.com/favicon_preferred1");
  const GURL icon_url_large("http://www.google.com/favicon_large");

  TestFaviconHandlerDelegate delegate(contents());
  Profile* profile = Profile::FromBrowserContext(
      contents()->GetBrowserContext());
  TestFaviconHandler handler(page_url, profile,
                             &delegate, FaviconHandler::FAVICON);

  handler.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = handler.history_handler();

  // Set valid icon data.
  history_handler->favicon_data_.known_icon = true;
  history_handler->favicon_data_.icon_type = history::FAVICON;
  history_handler->favicon_data_.expired = false;
  history_handler->favicon_data_.icon_url = icon_url;
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  history_handler->favicon_data_.image_data = data;

  // Send history response.
  history_handler->InvokeCallback();

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  // Note: the code will stop making requests when an icon matching the
  // preferred size is found, so icon_url_preferred must be last.
  urls.push_back(FaviconURL(icon_url_preferred1, FaviconURL::FAVICON));
  urls.push_back(FaviconURL(icon_url_large, FaviconURL::FAVICON));
  handler.OnUpdateFaviconURL(0, urls);
  EXPECT_EQ(2U, handler.image_urls().size());

  DownloadHandler* download_handler = handler.download_handler();

  // Download the first icon (set not in history).
  handler.history_handler()->favicon_data_.known_icon = false;
  handler.history_handler()->InvokeCallback();
  ASSERT_TRUE(download_handler->HasDownload());
  EXPECT_EQ(icon_url_preferred1, download_handler->GetImageUrl());
  download_handler->SetImageSize(gfx::kFaviconSize);
  download_handler->InvokeCallback();
  // Verify that this was detected as an exact match and image_urls_ is cleared.
  EXPECT_EQ(0U, handler.image_urls().size());

  // Verify correct icon size chosen.
  EXPECT_EQ(icon_url_preferred1, handler.GetEntry()->GetFavicon().url);
  EXPECT_TRUE(handler.GetEntry()->GetFavicon().valid);
  EXPECT_FALSE(handler.GetEntry()->GetFavicon().bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, handler.GetEntry()->GetFavicon().bitmap.width());
}

}  // namespace.
