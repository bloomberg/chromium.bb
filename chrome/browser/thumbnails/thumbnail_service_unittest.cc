// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/history/top_sites_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test ThumbnailServiceTest;

// A mock version of TopSitesImpl, used for testing
// ShouldAcquirePageThumbnail().
class MockTopSites : public history::TopSitesImpl {
 public:
  explicit MockTopSites(Profile* profile)
      : history::TopSitesImpl(profile),
        capacity_(1) {
  }

  // history::TopSitesImpl overrides.
  virtual bool IsNonForcedFull() OVERRIDE {
    return known_url_map_.size() >= capacity_;
  }
  virtual bool IsForcedFull() OVERRIDE {
    return false;
  }
  virtual bool IsKnownURL(const GURL& url) OVERRIDE {
    return known_url_map_.find(url.spec()) != known_url_map_.end();
  }
  virtual bool GetPageThumbnailScore(const GURL& url,
                                     ThumbnailScore* score) OVERRIDE {
    std::map<std::string, ThumbnailScore>::const_iterator iter =
        known_url_map_.find(url.spec());
    if (iter == known_url_map_.end()) {
      return false;
    } else {
      *score = iter->second;
      return true;
    }
  }

  // Adds a known URL with the associated thumbnail score.
  void AddKnownURL(const GURL& url, const ThumbnailScore& score) {
    known_url_map_[url.spec()] = score;
  }

 private:
  virtual ~MockTopSites() {}

  const size_t capacity_;
  std::map<std::string, ThumbnailScore> known_url_map_;

  DISALLOW_COPY_AND_ASSIGN(MockTopSites);
};

// A mock version of TestingProfile holds MockTopSites.
class MockProfile : public TestingProfile {
 public:
  MockProfile() : mock_top_sites_(new MockTopSites(this)) {
  }

  virtual history::TopSites* GetTopSites() OVERRIDE {
    return mock_top_sites_.get();
  }

  void AddKnownURL(const GURL& url, const ThumbnailScore& score) {
    mock_top_sites_->AddKnownURL(url, score);
  }

 private:
  scoped_refptr<MockTopSites> mock_top_sites_;

  DISALLOW_COPY_AND_ASSIGN(MockProfile);
};

TEST_F(ThumbnailServiceTest, ShouldUpdateThumbnail) {
  const GURL kGoodURL("http://www.google.com/");
  const GURL kBadURL("chrome://newtab");

  // Set up the mock profile along with mock top sites.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MockProfile profile;

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service(
      new thumbnails::ThumbnailServiceImpl(&profile));

  // Should be false because it's a bad URL.
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(kBadURL));

  // Should be true, as it's a good URL.
  EXPECT_TRUE(thumbnail_service->ShouldAcquirePageThumbnail(kGoodURL));

  // Not checking incognito mode since the service wouldn't have been created
  // in that case anyway.

  // Add a known URL. This makes the top sites data full.
  ThumbnailScore bad_score;
  bad_score.time_at_snapshot = base::Time::UnixEpoch();  // Ancient time stamp.
  profile.AddKnownURL(kGoodURL, bad_score);
  ASSERT_TRUE(profile.GetTopSites()->IsNonForcedFull());

  // Should be false, as the top sites data is full, and the new URL is
  // not known.
  const GURL kAnotherGoodURL("http://www.youtube.com/");
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(kAnotherGoodURL));

  // Should be true, as the existing thumbnail is bad (i.e. need a better one).
  EXPECT_TRUE(thumbnail_service->ShouldAcquirePageThumbnail(kGoodURL));

  // Replace the thumbnail score with a really good one.
  ThumbnailScore good_score;
  good_score.time_at_snapshot = base::Time::Now();  // Very new.
  good_score.at_top = true;
  good_score.good_clipping = true;
  good_score.boring_score = 0.0;
  good_score.load_completed = true;
  profile.AddKnownURL(kGoodURL, good_score);

  // Should be false, as the existing thumbnail is good enough (i.e. don't
  // need to replace the existing thumbnail which is new and good).
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(kGoodURL));
}
