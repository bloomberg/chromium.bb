// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_manager_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "content/public/test/test_utils.h"
#include "net/base/load_flags.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace suggestions {

const char kTestUrl1[] = "http://go.com/";
const char kTestUrl2[] = "http://goal.com/";
const char kTestBitmapUrl[] = "http://test.com";
const char kTestImagePath[] = "files/image_decoding/droids.png";
const char kInvalidImagePath[] = "files/DOESNOTEXIST";

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

using chrome::BitmapFetcher;
using content::BrowserThread;
using leveldb_proto::test::FakeDB;
using suggestions::ImageData;
using suggestions::ImageManagerImpl;

typedef base::hash_map<std::string, ImageData> EntryMap;

void AddEntry(const ImageData& d, EntryMap* map) { (*map)[d.url()] = d; }

class ImageManagerImplBrowserTest : public InProcessBrowserTest {
 public:
  ImageManagerImplBrowserTest()
      : num_callback_null_called_(0),
        num_callback_valid_called_(0),
        test_server_(net::SpawnedTestServer::TYPE_HTTP,
                     net::SpawnedTestServer::kLocalhost,
                     base::FilePath(kDocRoot)) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(test_server_.Start());
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    test_server_.Stop();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    fake_db_ = new FakeDB<ImageData>(&db_model_);
    image_manager_.reset(CreateImageManagerImpl(fake_db_));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    fake_db_ = NULL;
    db_model_.clear();
    image_manager_.reset();
    test_image_manager_.reset();
  }

  void InitializeTestBitmapData() {
    FakeDB<ImageData>* test_fake_db = new FakeDB<ImageData>(&db_model_);
    test_image_manager_.reset(CreateImageManagerImpl(test_fake_db));

    suggestions::SuggestionsProfile suggestions_profile;
    suggestions::ChromeSuggestion* suggestion =
        suggestions_profile.add_suggestions();
    suggestion->set_url(kTestBitmapUrl);
    suggestion->set_thumbnail(test_server_.GetURL(kTestImagePath).spec());

    test_image_manager_->Initialize(suggestions_profile);

    // Initialize empty database.
    test_fake_db->InitCallback(true);
    test_fake_db->LoadCallback(true);

    base::RunLoop run_loop;
    // Fetch existing URL.
    test_image_manager_->GetImageForURL(
        GURL(kTestBitmapUrl),
        base::Bind(&ImageManagerImplBrowserTest::OnTestImageAvailable,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }

  void OnTestImageAvailable(base::RunLoop* loop, const GURL& url,
                            const SkBitmap* bitmap) {
    CHECK(bitmap);
    // Copy the resource locally.
    test_bitmap_ = *bitmap;
    loop->Quit();
  }

  void InitializeDefaultImageMapAndDatabase(
      ImageManagerImpl* image_manager, FakeDB<ImageData>* fake_db) {
    CHECK(image_manager);
    CHECK(fake_db);

    suggestions::SuggestionsProfile suggestions_profile;
    suggestions::ChromeSuggestion* suggestion =
        suggestions_profile.add_suggestions();
    suggestion->set_url(kTestUrl1);
    suggestion->set_thumbnail(test_server_.GetURL(kTestImagePath).spec());

    image_manager->Initialize(suggestions_profile);

    // Initialize empty database.
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
  }

  ImageData GetSampleImageData(const std::string& url) {
    ImageData data;
    data.set_url(url);
    std::vector<unsigned char> encoded;
    EXPECT_TRUE(ImageManagerImpl::EncodeImage(test_bitmap_, &encoded));
    data.set_data(std::string(encoded.begin(), encoded.end()));
    return data;
  }

  void OnImageAvailable(base::RunLoop* loop, const GURL& url,
                            const SkBitmap* bitmap) {
    if (bitmap) {
      num_callback_valid_called_++;
      std::vector<unsigned char> actual;
      std::vector<unsigned char> expected;
      EXPECT_TRUE(ImageManagerImpl::EncodeImage(*bitmap, &actual));
      EXPECT_TRUE(ImageManagerImpl::EncodeImage(test_bitmap_, &expected));
      // Check first 100 bytes.
      std::string actual_str(actual.begin(), actual.begin() + 100);
      std::string expected_str(expected.begin(), expected.begin() + 100);
      EXPECT_EQ(expected_str, actual_str);
    } else {
      num_callback_null_called_++;
    }
    loop->Quit();
  }

  ImageManagerImpl* CreateImageManagerImpl(FakeDB<ImageData>* fake_db) {
    return new ImageManagerImpl(
        browser()->profile()->GetRequestContext(),
        scoped_ptr<leveldb_proto::ProtoDatabase<ImageData> >(fake_db),
        FakeDB<ImageData>::DirectoryForTestDB());
  }

  EntryMap db_model_;
  // Owned by the ImageManagerImpl under test.
  FakeDB<ImageData>* fake_db_;

  SkBitmap test_bitmap_;
  scoped_ptr<ImageManagerImpl> test_image_manager_;

  int num_callback_null_called_;
  int num_callback_valid_called_;
  net::SpawnedTestServer test_server_;
  // Under test.
  scoped_ptr<ImageManagerImpl> image_manager_;
};

IN_PROC_BROWSER_TEST_F(ImageManagerImplBrowserTest, GetImageForURLNetwork) {
  InitializeTestBitmapData();
  InitializeDefaultImageMapAndDatabase(image_manager_.get(), fake_db_);

  base::RunLoop run_loop;
  // Fetch existing URL.
  image_manager_->GetImageForURL(
      GURL(kTestUrl1),
      base::Bind(&ImageManagerImplBrowserTest::OnImageAvailable,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);

  base::RunLoop run_loop2;
  // Fetch non-existing URL.
  image_manager_->GetImageForURL(
      GURL(kTestUrl2),
      base::Bind(&ImageManagerImplBrowserTest::OnImageAvailable,
                 base::Unretained(this), &run_loop2));
  run_loop2.Run();

  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ImageManagerImplBrowserTest,
                       GetImageForURLNetworkMultiple) {
  InitializeTestBitmapData();
  InitializeDefaultImageMapAndDatabase(image_manager_.get(), fake_db_);

  // Fetch non-existing URL, and add more while request is in flight.
  base::RunLoop run_loop;
  for (int i = 0; i < 5; i++) {
    // Fetch existing URL.
    image_manager_->GetImageForURL(
        GURL(kTestUrl1),
        base::Bind(&ImageManagerImplBrowserTest::OnImageAvailable,
                   base::Unretained(this), &run_loop));
  }
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(5, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ImageManagerImplBrowserTest,
                       GetImageForURLNetworkInvalid) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  suggestion->set_thumbnail(test_server_.GetURL(kInvalidImagePath).spec());

  image_manager_->Initialize(suggestions_profile);

  // Database will be initialized and loaded without anything in it.
  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  base::RunLoop run_loop;
  // Fetch existing URL that has invalid image.
  image_manager_->GetImageForURL(
      GURL(kTestUrl1),
      base::Bind(&ImageManagerImplBrowserTest::OnImageAvailable,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(0, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ImageManagerImplBrowserTest,
                       GetImageForURLNetworkCacheHit) {
  InitializeTestBitmapData();

  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  // The URL we set is invalid, to show that it will fail from network.
  suggestion->set_thumbnail(test_server_.GetURL(kInvalidImagePath).spec());

  // Create the ImageManagerImpl with an added entry in the database.
  AddEntry(GetSampleImageData(kTestUrl1), &db_model_);
  FakeDB<ImageData>* fake_db = new FakeDB<ImageData>(&db_model_);
  image_manager_.reset(CreateImageManagerImpl(fake_db));
  image_manager_->Initialize(suggestions_profile);
  fake_db->InitCallback(true);
  fake_db->LoadCallback(true);
  // Expect something in the cache.
  SkBitmap* bitmap = image_manager_->GetBitmapFromCache(GURL(kTestUrl1));
  EXPECT_FALSE(bitmap->isNull());

  base::RunLoop run_loop;
  image_manager_->GetImageForURL(
      GURL(kTestUrl1),
      base::Bind(&ImageManagerImplBrowserTest::OnImageAvailable,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);
}

}  // namespace suggestions
