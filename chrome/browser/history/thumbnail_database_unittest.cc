// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/ref_counted_memory.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

// data we'll put into the thumbnail database
static const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";
static const unsigned char blob2[] =
    "goiwuegrqrcomizqyzkjalitbahxfjytrqvpqeroicxmnlkhlzunacxaneviawrtxcywhgef";
static const unsigned char blob3[] =
    "3716871354098370776510470746794707624107647054607467847164027";
const double kBoringness = 0.25;
const double kWorseBoringness = 0.50;
const double kBetterBoringness = 0.10;
const double kTotallyBoring = 1.0;

const int64 kPage1 = 1234;

}  // namespace

class ThumbnailDatabaseTest : public testing::Test {
 public:
  ThumbnailDatabaseTest() {
  }
  ~ThumbnailDatabaseTest() {
  }

 protected:
  virtual void SetUp() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.path().AppendASCII("TestThumbnails.db");
    new_file_name_ = temp_dir_.path().AppendASCII("TestFavicons.db");

    google_bitmap_.reset(
        gfx::JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  }

  scoped_ptr<SkBitmap> google_bitmap_;

  ScopedTempDir temp_dir_;
  FilePath file_name_;
  FilePath new_file_name_;
};

TEST_F(ThumbnailDatabaseTest, GetFaviconAfterMigrationToTopSites) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FavIconID id = db.AddFavIcon(url);
  base::Time time = base::Time::Now();
  db.SetFavIcon(id, favicon, time);
  EXPECT_TRUE(db.RenameAndDropThumbnails(file_name_, new_file_name_));

  base::Time time_out;
  std::vector<unsigned char> favicon_out;
  GURL url_out;
  EXPECT_TRUE(db.GetFavIcon(id, &time_out, &favicon_out, &url_out));
  EXPECT_EQ(url, url_out);
  EXPECT_EQ(time.ToTimeT(), time_out.ToTimeT());
  ASSERT_EQ(data.size(), favicon_out.size());
  EXPECT_TRUE(std::equal(data.begin(),
                         data.end(),
                         favicon_out.begin()));
}

}  // namespace history
