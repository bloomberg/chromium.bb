// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/thumbnail_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using leveldb_proto::test::FakeDB;
using suggestions::ThumbnailData;
using suggestions::ThumbnailManager;

typedef base::hash_map<std::string, ThumbnailData> EntryMap;

const char kTestUrl[] = "http://go.com/";
const char kTestThumbnailUrl[] = "http://thumb.com/anchor_download_test.png";

class ThumbnailManagerTest : public testing::Test {
 protected:
  ThumbnailManager* CreateThumbnailManager(Profile* profile) {
    FakeDB<ThumbnailData>* fake_db = new FakeDB<ThumbnailData>(&db_model_);
    return new ThumbnailManager(
        profile->GetRequestContext(),
        scoped_ptr<leveldb_proto::ProtoDatabase<ThumbnailData> >(fake_db),
        FakeDB<ThumbnailData>::DirectoryForTestDB());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  EntryMap db_model_;
};

}  // namespace

namespace suggestions {

TEST_F(ThumbnailManagerTest, InitializeTest) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl);
  suggestion->set_thumbnail(kTestThumbnailUrl);

  TestingProfile profile;
  scoped_ptr<ThumbnailManager> thumbnail_manager(
      CreateThumbnailManager(&profile));
  thumbnail_manager->Initialize(suggestions_profile);

  GURL output;
  EXPECT_TRUE(thumbnail_manager->GetThumbnailURL(GURL(kTestUrl), &output));
  EXPECT_EQ(GURL(kTestThumbnailUrl), output);

  EXPECT_FALSE(
      thumbnail_manager->GetThumbnailURL(GURL("http://b.com"), &output));
}

}  // namespace suggestions
