// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search/suggestions/image_manager_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using leveldb_proto::test::FakeDB;
using suggestions::ImageData;
using suggestions::ImageManagerImpl;

typedef base::hash_map<std::string, ImageData> EntryMap;

const char kTestUrl[] = "http://go.com/";
const char kTestImageUrl[] = "http://thumb.com/anchor_download_test.png";

class ImageManagerImplTest : public testing::Test {
 protected:
  ImageManagerImpl* CreateImageManager(Profile* profile) {
    FakeDB<ImageData>* fake_db = new FakeDB<ImageData>(&db_model_);
    return new ImageManagerImpl(
        profile->GetRequestContext(),
        scoped_ptr<leveldb_proto::ProtoDatabase<ImageData> >(fake_db),
        FakeDB<ImageData>::DirectoryForTestDB());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  EntryMap db_model_;
};

}  // namespace

namespace suggestions {

TEST_F(ImageManagerImplTest, InitializeTest) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl);
  suggestion->set_thumbnail(kTestImageUrl);

  TestingProfile profile;
  scoped_ptr<ImageManagerImpl> image_manager(
      CreateImageManager(&profile));
  image_manager->Initialize(suggestions_profile);

  GURL output;
  EXPECT_TRUE(image_manager->GetImageURL(GURL(kTestUrl), &output));
  EXPECT_EQ(GURL(kTestImageUrl), output);

  EXPECT_FALSE(image_manager->GetImageURL(GURL("http://b.com"), &output));
}

}  // namespace suggestions
