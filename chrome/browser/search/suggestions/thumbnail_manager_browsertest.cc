// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/thumbnail_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
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
using suggestions::ThumbnailData;
using suggestions::ThumbnailManager;

typedef base::hash_map<std::string, ThumbnailData> EntryMap;

void AddEntry(const ThumbnailData& d, EntryMap* map) { (*map)[d.url()] = d; }

class ThumbnailManagerBrowserTest : public InProcessBrowserTest {
 public:
  ThumbnailManagerBrowserTest()
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
    fake_db_ = new FakeDB<ThumbnailData>(&db_model_);
    thumbnail_manager_.reset(CreateThumbnailManager(fake_db_));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    fake_db_ = NULL;
    db_model_.clear();
    thumbnail_manager_.reset();
    test_thumbnail_manager_.reset();
  }

  void InitializeTestBitmapData() {
    FakeDB<ThumbnailData>* test_fake_db = new FakeDB<ThumbnailData>(&db_model_);
    test_thumbnail_manager_.reset(CreateThumbnailManager(test_fake_db));

    suggestions::SuggestionsProfile suggestions_profile;
    suggestions::ChromeSuggestion* suggestion =
        suggestions_profile.add_suggestions();
    suggestion->set_url(kTestBitmapUrl);
    suggestion->set_thumbnail(test_server_.GetURL(kTestImagePath).spec());

    test_thumbnail_manager_->Initialize(suggestions_profile);

    // Initialize empty database.
    test_fake_db->InitCallback(true);
    test_fake_db->LoadCallback(true);

    base::RunLoop run_loop;
    // Fetch existing URL.
    test_thumbnail_manager_->GetImageForURL(
        GURL(kTestBitmapUrl),
        base::Bind(&ThumbnailManagerBrowserTest::OnTestThumbnailAvailable,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }

  void OnTestThumbnailAvailable(base::RunLoop* loop, const GURL& url,
                                const SkBitmap* bitmap) {
    CHECK(bitmap);
    // Copy the resource locally.
    test_bitmap_ = *bitmap;
    loop->Quit();
  }

  void InitializeDefaultThumbnailMapAndDatabase(
      ThumbnailManager* thumbnail_manager, FakeDB<ThumbnailData>* fake_db) {
    CHECK(thumbnail_manager);
    CHECK(fake_db);

    suggestions::SuggestionsProfile suggestions_profile;
    suggestions::ChromeSuggestion* suggestion =
        suggestions_profile.add_suggestions();
    suggestion->set_url(kTestUrl1);
    suggestion->set_thumbnail(test_server_.GetURL(kTestImagePath).spec());

    thumbnail_manager->Initialize(suggestions_profile);

    // Initialize empty database.
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
  }

  ThumbnailData GetSampleThumbnailData(const std::string& url) {
    ThumbnailData data;
    data.set_url(url);
    std::vector<unsigned char> encoded;
    EXPECT_TRUE(ThumbnailManager::EncodeThumbnail(test_bitmap_, &encoded));
    data.set_data(std::string(encoded.begin(), encoded.end()));
    return data;
  }

  void OnThumbnailAvailable(base::RunLoop* loop, const GURL& url,
                            const SkBitmap* bitmap) {
    if (bitmap) {
      num_callback_valid_called_++;
      /*std::vector<unsigned char> actual;
      std::vector<unsigned char> expected;
      EXPECT_TRUE(ThumbnailManager::EncodeThumbnail(*bitmap, &actual));
      EXPECT_TRUE(ThumbnailManager::EncodeThumbnail(test_bitmap_, &expected));
      // Check first 100 bytes.
      std::string actual_str(actual.begin(), actual.begin() + 100);
      std::string expected_str(expected.begin(), expected.begin() + 100);
      EXPECT_EQ(expected_str, actual_str);*/
    } else {
      num_callback_null_called_++;
    }
    loop->Quit();
  }

  ThumbnailManager* CreateThumbnailManager(FakeDB<ThumbnailData>* fake_db) {
    return new ThumbnailManager(
        browser()->profile()->GetRequestContext(),
        scoped_ptr<leveldb_proto::ProtoDatabase<ThumbnailData> >(fake_db),
        FakeDB<ThumbnailData>::DirectoryForTestDB());
  }

  EntryMap db_model_;
  // Owned by the ThumbnailManager under test.
  FakeDB<ThumbnailData>* fake_db_;

  SkBitmap test_bitmap_;
  scoped_ptr<ThumbnailManager> test_thumbnail_manager_;

  int num_callback_null_called_;
  int num_callback_valid_called_;
  net::SpawnedTestServer test_server_;
  // Under test.
  scoped_ptr<ThumbnailManager> thumbnail_manager_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest, GetImageForURLNetwork) {
  InitializeDefaultThumbnailMapAndDatabase(thumbnail_manager_.get(), fake_db_);

  base::RunLoop run_loop;
  // Fetch existing URL.
  thumbnail_manager_->GetImageForURL(
      GURL(kTestUrl1),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);

  base::RunLoop run_loop2;
  // Fetch non-existing URL.
  thumbnail_manager_->GetImageForURL(
      GURL(kTestUrl2),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this), &run_loop2));
  run_loop2.Run();

  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest,
                       GetImageForURLNetworkMultiple) {
  InitializeDefaultThumbnailMapAndDatabase(thumbnail_manager_.get(), fake_db_);

  // Fetch non-existing URL, and add more while request is in flight.
  base::RunLoop run_loop;
  for (int i = 0; i < 5; i++) {
    // Fetch existing URL.
    thumbnail_manager_->GetImageForURL(
        GURL(kTestUrl1),
        base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                   base::Unretained(this), &run_loop));
  }
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(5, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest,
                       GetImageForURLNetworkInvalid) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  suggestion->set_thumbnail(test_server_.GetURL(kInvalidImagePath).spec());

  thumbnail_manager_->Initialize(suggestions_profile);

  // Database will be initialized and loaded without anything in it.
  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  base::RunLoop run_loop;
  // Fetch existing URL that has invalid thumbnail.
  thumbnail_manager_->GetImageForURL(
      GURL(kTestUrl1),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(0, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest,
                       GetImageForURLNetworkCacheHit) {
  InitializeTestBitmapData();

  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  // The URL we set is invalid, to show that it will fail from network.
  suggestion->set_thumbnail(test_server_.GetURL(kInvalidImagePath).spec());

  // Create the ThumbnailManager with an added entry in the database.
  AddEntry(GetSampleThumbnailData(kTestUrl1), &db_model_);
  FakeDB<ThumbnailData>* fake_db = new FakeDB<ThumbnailData>(&db_model_);
  thumbnail_manager_.reset(CreateThumbnailManager(fake_db));
  thumbnail_manager_->Initialize(suggestions_profile);
  fake_db->InitCallback(true);
  fake_db->LoadCallback(true);
  // Expect something in the cache.
  SkBitmap* bitmap = thumbnail_manager_->GetBitmapFromCache(GURL(kTestUrl1));
  EXPECT_FALSE(bitmap->isNull());

  base::RunLoop run_loop;
  thumbnail_manager_->GetImageForURL(
      GURL(kTestUrl1),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);
}

}  // namespace suggestions
