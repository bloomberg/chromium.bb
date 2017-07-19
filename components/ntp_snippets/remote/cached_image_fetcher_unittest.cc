// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/cached_image_fetcher.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using testing::Eq;
using testing::Property;

namespace ntp_snippets {

namespace {

const char kImageData[] = "data";
const char kImageURL[] = "http://image.test/test.png";
const char kSnippetID[] = "http://localhost";

// Always decodes a valid image for all non-empty input.
class FakeImageDecoder : public image_fetcher::ImageDecoder {
 public:
  void DecodeImage(
      const std::string& image_data,
      const gfx::Size& desired_image_frame_size,
      const image_fetcher::ImageDecodedCallback& callback) override {
    gfx::Image image;
    if (!image_data.empty()) {
      image = gfx::test::CreateImage();
    }
    callback.Run(image);
  }
};

}  // namespace

class CachedImageFetcherTest : public testing::Test {
 public:
  CachedImageFetcherTest()
      : fake_url_fetcher_factory_(nullptr),
        mock_task_runner_(new base::TestSimpleTaskRunner()),
        mock_task_runner_handle_(mock_task_runner_) {
    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());

    RequestThrottler::RegisterProfilePrefs(pref_service_.registry());
    database_ = base::MakeUnique<RemoteSuggestionsDatabase>(
        database_dir_.GetPath(), mock_task_runner_);
    request_context_getter_ = scoped_refptr<net::TestURLRequestContextGetter>(
        new net::TestURLRequestContextGetter(mock_task_runner_.get()));

    auto decoder = base::MakeUnique<FakeImageDecoder>();
    fake_image_decoder_ = decoder.get();
    cached_image_fetcher_ = base::MakeUnique<ntp_snippets::CachedImageFetcher>(
        base::MakeUnique<image_fetcher::ImageFetcherImpl>(
            std::move(decoder), request_context_getter_.get()),
        &pref_service_, database_.get());
    RunUntilIdle();
    EXPECT_TRUE(database_->IsInitialized());
  }

  void FetchImage(const ImageFetchedCallback& callback) {
    ContentSuggestion::ID content_suggestion_id(
        Category::FromKnownCategory(KnownCategories::ARTICLES), kSnippetID);
    cached_image_fetcher_->FetchSuggestionImage(content_suggestion_id,
                                                GURL(kImageURL), callback);
  }

  void RunUntilIdle() { mock_task_runner_->RunUntilIdle(); }

  RemoteSuggestionsDatabase* database() { return database_.get(); }
  FakeImageDecoder* fake_image_decoder() { return fake_image_decoder_; }
  net::FakeURLFetcherFactory* fake_url_fetcher_factory() {
    return &fake_url_fetcher_factory_;
  }

 private:
  std::unique_ptr<CachedImageFetcher> cached_image_fetcher_;
  std::unique_ptr<RemoteSuggestionsDatabase> database_;
  base::ScopedTempDir database_dir_;
  FakeImageDecoder* fake_image_decoder_;
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  scoped_refptr<base::TestSimpleTaskRunner> mock_task_runner_;
  base::ThreadTaskRunnerHandle mock_task_runner_handle_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherTest);
};

TEST_F(CachedImageFetcherTest, FetchImageFromCache) {
  // Save the image in the database.
  database()->SaveImage(kSnippetID, kImageData);
  RunUntilIdle();

  // Do not provide any URL responses and expect that the image is fetched (from
  // cache).
  base::MockCallback<ImageFetchedCallback> mock_image_fetched_callback;
  EXPECT_CALL(mock_image_fetched_callback,
              Run(Property(&gfx::Image::IsEmpty, Eq(false))));
  FetchImage(mock_image_fetched_callback.Get());
  RunUntilIdle();
}

TEST_F(CachedImageFetcherTest, FetchImageNotInCache) {
  // Expect the image to be fetched by URL.
  fake_url_fetcher_factory()->SetFakeResponse(GURL(kImageURL), kImageData,
                                              net::HTTP_OK,
                                              net::URLRequestStatus::SUCCESS);
  base::MockCallback<ImageFetchedCallback> mock_image_fetched_callback;
  EXPECT_CALL(mock_image_fetched_callback,
              Run(Property(&gfx::Image::IsEmpty, Eq(false))));
  FetchImage(mock_image_fetched_callback.Get());
  RunUntilIdle();
}

TEST_F(CachedImageFetcherTest, FetchNonExistingImage) {
  const std::string kErrorResponse = "error-response";
  fake_url_fetcher_factory()->SetFakeResponse(GURL(kImageURL), kErrorResponse,
                                              net::HTTP_NOT_FOUND,
                                              net::URLRequestStatus::FAILED);
  // Expect an empty image is fetched if the URL cannot be requested.
  const std::string kEmptyImageData;
  base::MockCallback<ImageFetchedCallback> mock_image_fetched_callback;
  EXPECT_CALL(mock_image_fetched_callback,
              Run(Property(&gfx::Image::IsEmpty, Eq(true))));
  FetchImage(mock_image_fetched_callback.Get());
  RunUntilIdle();
}

}  // namespace ntp_snippets
