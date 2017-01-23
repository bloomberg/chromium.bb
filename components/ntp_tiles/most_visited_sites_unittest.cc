// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <vector>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_tiles {

// Defined for googletest. Must be defined in the same namespace.
void PrintTo(const NTPTile& tile, std::ostream* os) {
  *os << "{\"" << tile.title << "\", \"" << tile.url << "\", "
      << static_cast<int>(tile.source) << "}";
}

namespace {

using history::MostVisitedURL;
using history::MostVisitedURLList;
using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;
using testing::AtLeast;
using testing::ByMove;
using testing::ElementsAre;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SizeIs;
using testing::StrictMock;
using testing::_;

MATCHER_P3(MatchesTile,
           title,
           url,
           source,
           std::string("has title \"") + title + std::string("\" and url \"") +
               url +
               std::string("\" and source ") +
               testing::PrintToString(static_cast<int>(source))) {
  return arg.title == base::ASCIIToUTF16(title) && arg.url == GURL(url) &&
         arg.source == source;
}

NTPTile MakeTile(const std::string& title,
                 const std::string& url,
                 NTPTileSource source) {
  NTPTile tile;
  tile.title = base::ASCIIToUTF16(title);
  tile.url = GURL(url);
  tile.source = source;
  return tile;
}

ChromeSuggestion MakeSuggestion(const std::string& title,
                                const std::string& url) {
  ChromeSuggestion suggestion;
  suggestion.set_title(title);
  suggestion.set_url(url);
  return suggestion;
}

SuggestionsProfile MakeProfile(
    const std::vector<ChromeSuggestion>& suggestions) {
  SuggestionsProfile profile;
  for (const ChromeSuggestion& suggestion : suggestions) {
    *profile.add_suggestions() = suggestion;
  }
  return profile;
}

MostVisitedURL MakeMostVisitedURL(const std::string& title,
                                  const std::string& url) {
  MostVisitedURL result;
  result.title = base::ASCIIToUTF16(title);
  result.url = GURL(url);
  return result;
}

class MockTopSites : public TopSites {
 public:
  MOCK_METHOD0(ShutdownOnUIThread, void());
  MOCK_METHOD3(SetPageThumbnail,
               bool(const GURL& url,
                    const gfx::Image& thumbnail,
                    const ThumbnailScore& score));
  MOCK_METHOD3(SetPageThumbnailToJPEGBytes,
               bool(const GURL& url,
                    const base::RefCountedMemory* memory,
                    const ThumbnailScore& score));
  MOCK_METHOD2(GetMostVisitedURLs,
               void(const GetMostVisitedURLsCallback& callback,
                    bool include_forced_urls));
  MOCK_METHOD3(GetPageThumbnail,
               bool(const GURL& url,
                    bool prefix_match,
                    scoped_refptr<base::RefCountedMemory>* bytes));
  MOCK_METHOD2(GetPageThumbnailScore,
               bool(const GURL& url, ThumbnailScore* score));
  MOCK_METHOD2(GetTemporaryPageThumbnailScore,
               bool(const GURL& url, ThumbnailScore* score));
  MOCK_METHOD0(SyncWithHistory, void());
  MOCK_CONST_METHOD0(HasBlacklistedItems, bool());
  MOCK_METHOD1(AddBlacklistedURL, void(const GURL& url));
  MOCK_METHOD1(RemoveBlacklistedURL, void(const GURL& url));
  MOCK_METHOD1(IsBlacklisted, bool(const GURL& url));
  MOCK_METHOD0(ClearBlacklistedURLs, void());
  MOCK_METHOD0(StartQueryForMostVisited, base::CancelableTaskTracker::TaskId());
  MOCK_METHOD1(IsKnownURL, bool(const GURL& url));
  MOCK_CONST_METHOD1(GetCanonicalURLString,
                     const std::string&(const GURL& url));
  MOCK_METHOD0(IsNonForcedFull, bool());
  MOCK_METHOD0(IsForcedFull, bool());
  MOCK_CONST_METHOD0(loaded, bool());
  MOCK_METHOD0(GetPrepopulatedPages, history::PrepopulatedPageList());
  MOCK_METHOD2(AddForcedURL, bool(const GURL& url, const base::Time& time));
  MOCK_METHOD1(OnNavigationCommitted, void(const GURL& url));

 protected:
  ~MockTopSites() override = default;
};

class MockSuggestionsService : public SuggestionsService {
 public:
  MOCK_METHOD0(FetchSuggestionsData, bool());
  MOCK_CONST_METHOD0(GetSuggestionsDataFromCache,
                     base::Optional<SuggestionsProfile>());
  MOCK_METHOD1(AddCallback,
               std::unique_ptr<ResponseCallbackList::Subscription>(
                   const ResponseCallback& callback));
  MOCK_METHOD2(GetPageThumbnail,
               void(const GURL& url, const BitmapCallback& callback));
  MOCK_METHOD3(GetPageThumbnailWithURL,
               void(const GURL& url,
                    const GURL& thumbnail_url,
                    const BitmapCallback& callback));
  MOCK_METHOD1(BlacklistURL, bool(const GURL& candidate_url));
  MOCK_METHOD1(UndoBlacklistURL, bool(const GURL& url));
  MOCK_METHOD0(ClearBlacklist, void());
};

class MockPopularSites : public PopularSites {
 public:
  MOCK_METHOD2(StartFetch,
               void(bool force_download, const FinishedCallback& callback));
  MOCK_CONST_METHOD0(sites, const SitesVector&());
  MOCK_CONST_METHOD0(GetLastURLFetched, GURL());
  MOCK_METHOD0(GetURLToFetch, GURL());
  MOCK_METHOD0(GetCountryToFetch, std::string());
  MOCK_METHOD0(GetVersionToFetch, std::string());
  MOCK_METHOD0(GetCachedJson, const base::ListValue*());
};

class MockMostVisitedSitesObserver : public MostVisitedSites::Observer {
 public:
  MOCK_METHOD1(OnMostVisitedURLsAvailable, void(const NTPTilesVector& tiles));
  MOCK_METHOD1(OnIconMadeAvailable, void(const GURL& site_url));
};

// CallbackList-like container without Subscription, mimic-ing the
// implementation in TopSites (which doesn't use base::CallbackList).
class TopSitesCallbackList {
 public:
  // Second argument declared to match the signature of GetMostVisitedURLs().
  void Add(const TopSites::GetMostVisitedURLsCallback& callback,
           testing::Unused) {
    callbacks_.push_back(callback);
  }

  void ClearAndNotify(const MostVisitedURLList& list) {
    std::vector<TopSites::GetMostVisitedURLsCallback> callbacks;
    callbacks.swap(callbacks_);
    for (const auto& callback : callbacks) {
      callback.Run(list);
    }
  }

  bool empty() const { return callbacks_.empty(); }

 private:
  std::vector<TopSites::GetMostVisitedURLsCallback> callbacks_;
};

class MostVisitedSitesTest : public ::testing::Test {
 protected:
  MostVisitedSitesTest()
      : mock_top_sites_(new StrictMock<MockTopSites>()),
        mock_popular_sites_(new StrictMock<MockPopularSites>()),
        most_visited_sites_(&pref_service_,
                            mock_top_sites_,
                            &mock_suggestions_service_,
                            base::WrapUnique(mock_popular_sites_),
                            /*icon_cacher=*/nullptr,
                            /*supervisor=*/nullptr) {
    MostVisitedSites::RegisterProfilePrefs(pref_service_.registry());
    // TODO(mastiz): Add test coverage including Popular Sites.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableNTPPopularSites);
    // PopularSites::sites() might be called even if the feature is disabled.
    // An empty vector is returned because there was no actual fetch.
    EXPECT_CALL(*mock_popular_sites_, sites())
        .Times(AtLeast(0))
        .WillRepeatedly(ReturnRef(empty_popular_sites_vector_));
  }

  bool VerifyAndClearExpectations() {
    base::RunLoop().RunUntilIdle();
    // Note that we don't verify or clear mock_popular_sites_, since there's
    // no interaction expected except sites() possibly being called, which is
    // verified during teardown.
    return Mock::VerifyAndClearExpectations(mock_top_sites_.get()) &&
           Mock::VerifyAndClearExpectations(&mock_suggestions_service_) &&
           Mock::VerifyAndClearExpectations(&mock_observer_);
  }

  base::CallbackList<SuggestionsService::ResponseCallback::RunType>
      suggestions_service_callbacks_;
  TopSitesCallbackList top_sites_callbacks_;

  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<StrictMock<MockTopSites>> mock_top_sites_;
  StrictMock<MockSuggestionsService> mock_suggestions_service_;
  StrictMock<MockPopularSites>* const mock_popular_sites_;
  StrictMock<MockMostVisitedSitesObserver> mock_observer_;
  MostVisitedSites most_visited_sites_;
  const PopularSites::SitesVector empty_popular_sites_vector_;
};

TEST_F(MostVisitedSitesTest, ShouldStartNoCallInConstructor) {
  // No call to mocks expected by the mere fact of instantiating
  // MostVisitedSites.
  base::RunLoop().RunUntilIdle();
}

class MostVisitedSitesWithCacheHitTest : public MostVisitedSitesTest {
 public:
  // Constructor sets the common expectations for the case where suggestions
  // service has cached results when the observer is registered.
  MostVisitedSitesWithCacheHitTest() {
    InSequence seq;
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
        .WillOnce(Invoke(&suggestions_service_callbacks_,
                         &SuggestionsService::ResponseCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
        .WillOnce(Return(MakeProfile({
            MakeSuggestion("Site 1", "http://site1/"),
            MakeSuggestion("Site 2", "http://site2/"),
            MakeSuggestion("Site 3", "http://site3/"),
            MakeSuggestion("Site 4", "http://site4/"),
        })));
    EXPECT_CALL(mock_observer_,
                OnMostVisitedURLsAvailable(ElementsAre(
                    MatchesTile("Site 1", "http://site1/",
                                NTPTileSource::SUGGESTIONS_SERVICE),
                    MatchesTile("Site 2", "http://site2/",
                                NTPTileSource::SUGGESTIONS_SERVICE),
                    MatchesTile("Site 3", "http://site3/",
                                NTPTileSource::SUGGESTIONS_SERVICE))));
    EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
        .WillOnce(Return(true));
    most_visited_sites_.SetMostVisitedURLsObserver(&mock_observer_,
                                                   /*num_sites=*/3);
    VerifyAndClearExpectations();

    EXPECT_FALSE(suggestions_service_callbacks_.empty());
    EXPECT_TRUE(top_sites_callbacks_.empty());
  }
};

TEST_F(MostVisitedSitesWithCacheHitTest, ShouldFavorSuggestionsServiceCache) {
  // Constructor sets basic expectations for a suggestions service cache hit.
}

TEST_F(MostVisitedSitesWithCacheHitTest,
       ShouldPropagateUpdateBySuggestionsService) {
  EXPECT_CALL(
      mock_observer_,
      OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
          "Site 5", "http://site5/", NTPTileSource::SUGGESTIONS_SERVICE))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 5", "http://site5/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MostVisitedSitesWithCacheHitTest,
       ShouldSwitchToTopSitesIfEmptyUpdateBySuggestionsService) {
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_, false))
      .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();

  EXPECT_CALL(mock_observer_,
              OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
                  "Site 5", "http://site5/", NTPTileSource::TOP_SITES))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 5", "http://site5/")});
  base::RunLoop().RunUntilIdle();
}

class MostVisitedSitesWithEmptyCacheTest : public MostVisitedSitesTest {
 public:
  // Constructor sets the common expectations for the case where suggestions
  // service doesn't have cached results when the observer is registered.
  MostVisitedSitesWithEmptyCacheTest() {
    InSequence seq;
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
        .WillOnce(Invoke(&suggestions_service_callbacks_,
                         &SuggestionsService::ResponseCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
        .WillOnce(Return(SuggestionsProfile()));  // Empty cache.
    EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_, false))
        .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
        .WillOnce(Return(true));
    most_visited_sites_.SetMostVisitedURLsObserver(&mock_observer_,
                                                   /*num_sites=*/3);
    VerifyAndClearExpectations();

    EXPECT_FALSE(suggestions_service_callbacks_.empty());
    EXPECT_FALSE(top_sites_callbacks_.empty());
  }
};

TEST_F(MostVisitedSitesWithEmptyCacheTest,
       ShouldQueryTopSitesAndSuggestionsService) {
  // Constructor sets basic expectations for a suggestions service cache miss.
}

TEST_F(MostVisitedSitesWithEmptyCacheTest,
       ShouldIgnoreTopSitesIfSuggestionsServiceFaster) {
  // Reply from suggestions service triggers and update to our observer.
  EXPECT_CALL(
      mock_observer_,
      OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
          "Site 1", "http://site1/", NTPTileSource::SUGGESTIONS_SERVICE))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 1", "http://site1/")}));
  VerifyAndClearExpectations();

  // Reply from top sites is ignored (i.e. not reported to observer).
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 2", "http://site2/")});
  base::RunLoop().RunUntilIdle();
}

TEST_F(MostVisitedSitesWithEmptyCacheTest,
       ShouldExposeTopSitesIfSuggestionsServiceFasterButEmpty) {
  // Empty reply from suggestions service causes no update to our observer.
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();

  // Reply from top sites is propagated to observer.
  // TODO(mastiz): Avoid a second redundant call to the observer.
  EXPECT_CALL(mock_observer_,
              OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
                  "Site 1", "http://site1/", NTPTileSource::TOP_SITES))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/")});
  base::RunLoop().RunUntilIdle();
}

TEST_F(MostVisitedSitesWithEmptyCacheTest,
       ShouldFavorSuggestionsServiceAlthoughSlower) {
  // Reply from top sites is propagated to observer.
  EXPECT_CALL(mock_observer_,
              OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
                  "Site 1", "http://site1/", NTPTileSource::TOP_SITES))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/")});
  VerifyAndClearExpectations();

  // Reply from suggestions service overrides top sites.
  InSequence seq;
  EXPECT_CALL(
      mock_observer_,
      OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
          "Site 2", "http://site2/", NTPTileSource::SUGGESTIONS_SERVICE))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 2", "http://site2/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MostVisitedSitesWithEmptyCacheTest,
       ShouldIgnoreSuggestionsServiceIfSlowerAndEmpty) {
  // Reply from top sites is propagated to observer.
  EXPECT_CALL(mock_observer_,
              OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
                  "Site 1", "http://site1/", NTPTileSource::TOP_SITES))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/")});
  VerifyAndClearExpectations();

  // Reply from suggestions service is empty and thus ignored. However, the
  // current implementation issues a redundant query to TopSites.
  // TODO(mastiz): Avoid this redundant call.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_, false))
      .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MostVisitedSitesWithEmptyCacheTest, ShouldPropagateUpdateByTopSites) {
  // Reply from top sites is propagated to observer.
  EXPECT_CALL(mock_observer_,
              OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
                  "Site 1", "http://site1/", NTPTileSource::TOP_SITES))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/")});
  VerifyAndClearExpectations();

  // Reply from suggestions service is empty and thus ignored. However, the
  // current implementation issues a redundant query to TopSites.
  // TODO(mastiz): Avoid this redundant call.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_, false))
      .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();

  // Update from top sites is propagated to observer.
  EXPECT_CALL(mock_observer_,
              OnMostVisitedURLsAvailable(ElementsAre(MatchesTile(
                  "Site 2", "http://site2/", NTPTileSource::TOP_SITES))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 2", "http://site2/")});
  base::RunLoop().RunUntilIdle();
}

// This a test for MostVisitedSites::MergeTiles(...) method, and thus has the
// same scope as the method itself. This tests merging popular sites with
// personal tiles.
// More important things out of the scope of testing presently:
// - Removing blacklisted tiles.
// - Correct host extraction from the URL.
// - Ensuring personal tiles are not duplicated in popular tiles.
TEST(MostVisitedSitesMergeTest, ShouldMergeTilesWithPersonalOnly) {
  std::vector<NTPTile> personal_tiles{
      MakeTile("Site 1", "https://www.site1.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 2", "https://www.site2.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 3", "https://www.site3.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 4", "https://www.site4.com/", NTPTileSource::TOP_SITES),
  };
  // Without any popular tiles, the result after merge should be the personal
  // tiles.
  EXPECT_THAT(MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                           /*whitelist_tiles=*/NTPTilesVector(),
                                           /*popular_tiles=*/NTPTilesVector()),
              ElementsAre(MatchesTile("Site 1", "https://www.site1.com/",
                                      NTPTileSource::TOP_SITES),
                          MatchesTile("Site 2", "https://www.site2.com/",
                                      NTPTileSource::TOP_SITES),
                          MatchesTile("Site 3", "https://www.site3.com/",
                                      NTPTileSource::TOP_SITES),
                          MatchesTile("Site 4", "https://www.site4.com/",
                                      NTPTileSource::TOP_SITES)));
}

TEST(MostVisitedSitesMergeTest, ShouldMergeTilesWithPopularOnly) {
  std::vector<NTPTile> popular_tiles{
      MakeTile("Site 1", "https://www.site1.com/", NTPTileSource::POPULAR),
      MakeTile("Site 2", "https://www.site2.com/", NTPTileSource::POPULAR),
      MakeTile("Site 3", "https://www.site3.com/", NTPTileSource::POPULAR),
      MakeTile("Site 4", "https://www.site4.com/", NTPTileSource::POPULAR),
  };
  // Without any personal tiles, the result after merge should be the popular
  // tiles.
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(/*personal_tiles=*/NTPTilesVector(),
                                   /*whitelist_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles)),
      ElementsAre(MatchesTile("Site 1", "https://www.site1.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 2", "https://www.site2.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 3", "https://www.site3.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 4", "https://www.site4.com/",
                              NTPTileSource::POPULAR)));
}

TEST(MostVisitedSitesMergeTest, ShouldMergeTilesFavoringPersonalOverPopular) {
  std::vector<NTPTile> popular_tiles{
      MakeTile("Site 1", "https://www.site1.com/", NTPTileSource::POPULAR),
      MakeTile("Site 2", "https://www.site2.com/", NTPTileSource::POPULAR),
  };
  std::vector<NTPTile> personal_tiles{
      MakeTile("Site 3", "https://www.site3.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 4", "https://www.site4.com/", NTPTileSource::TOP_SITES),
  };
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                   /*whitelist_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles)),
      ElementsAre(MatchesTile("Site 3", "https://www.site3.com/",
                              NTPTileSource::TOP_SITES),
                  MatchesTile("Site 4", "https://www.site4.com/",
                              NTPTileSource::TOP_SITES),
                  MatchesTile("Site 1", "https://www.site1.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 2", "https://www.site2.com/",
                              NTPTileSource::POPULAR)));
}

}  // namespace
}  // namespace ntp_tiles
