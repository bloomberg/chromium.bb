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

using testing::_;
using testing::Eq;

namespace ntp_snippets {

namespace {

const char kImageData[] = "data";
const char kImageURL[] = "http://image.test/test.png";
const char kSnippetID[] = "http://localhost";

class MockImageDecoder : public image_fetcher::ImageDecoder {
 public:
  MOCK_METHOD3(DecodeImage,
               void(const std::string& image_data,
                    const gfx::Size& desired_image_frame_size,
                    const image_fetcher::ImageDecodedCallback& callback));
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

    auto decoder = base::MakeUnique<MockImageDecoder>();
    mock_image_decoder_ = decoder.get();
    cached_image_fetcher_ = base::MakeUnique<ntp_snippets::CachedImageFetcher>(
        base::MakeUnique<image_fetcher::ImageFetcherImpl>(
            std::move(decoder), request_context_getter_.get()),
        &pref_service_, database_.get());
    RunUntilIdle();
    EXPECT_TRUE(database_->IsInitialized());
  }

  void FetchImage() {
    ContentSuggestion::ID content_suggestion_id(
        Category::FromKnownCategory(KnownCategories::ARTICLES), kSnippetID);
    cached_image_fetcher_->FetchSuggestionImage(
        content_suggestion_id, GURL(kImageURL),
        base::Bind(&CachedImageFetcherTest::OnImageFetched,
                   base::Unretained(this)));
  }

  // TODO(gaschler): add a test where decoder runs this callback
  void OnImageFetched(const gfx::Image&) {}

  void RunUntilIdle() { mock_task_runner_->RunUntilIdle(); }

  RemoteSuggestionsDatabase* database() { return database_.get(); }
  net::FakeURLFetcherFactory* fake_url_fetcher_factory() {
    return &fake_url_fetcher_factory_;
  }
  MockImageDecoder* mock_image_decoder() { return mock_image_decoder_; }

 private:
  std::unique_ptr<CachedImageFetcher> cached_image_fetcher_;
  std::unique_ptr<RemoteSuggestionsDatabase> database_;
  base::ScopedTempDir database_dir_;
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  MockImageDecoder* mock_image_decoder_;
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
  EXPECT_CALL(*mock_image_decoder(), DecodeImage(kImageData, _, _));
  FetchImage();
  RunUntilIdle();
}

TEST_F(CachedImageFetcherTest, FetchImageNotInCache) {
  // Expect the image to be fetched by URL.
  fake_url_fetcher_factory()->SetFakeResponse(GURL(kImageURL), kImageData,
                                              net::HTTP_OK,
                                              net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*mock_image_decoder(), DecodeImage(kImageData, _, _));
  FetchImage();
  RunUntilIdle();
}

TEST_F(CachedImageFetcherTest, FetchNonExistingImage) {
  const std::string kErrorResponse = "error-response";
  fake_url_fetcher_factory()->SetFakeResponse(GURL(kImageURL), kErrorResponse,
                                              net::HTTP_NOT_FOUND,
                                              net::URLRequestStatus::FAILED);
  // Expect that decoder is called even if URL cannot be requested,
  // then with empty image data.
  const std::string kEmptyImageData;
  EXPECT_CALL(*mock_image_decoder(), DecodeImage(kEmptyImageData, _, _));
  FetchImage();
  RunUntilIdle();
}

}  // namespace ntp_snippets
