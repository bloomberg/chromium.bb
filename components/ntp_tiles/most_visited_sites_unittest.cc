// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/ntp_tiles/custom_links_manager.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/section_type.h"
#include "components/ntp_tiles/switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
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
using testing::AllOf;
using testing::AnyNumber;
using testing::ByMove;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::Ge;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Key;
using testing::Mock;
using testing::Not;
using testing::Pair;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::SizeIs;
using testing::StrictMock;
using testing::_;

const char kHomepageUrl[] = "http://homepa.ge/";
const char kHomepageTitle[] = "Homepage";
const char kTestExploreUrl[] = "https://example.com/";
const char kTestExploreTitle[] = "Example";

std::string PrintTile(const std::string& title,
                      const std::string& url,
                      TileSource source) {
  return std::string("has title \"") + title + std::string("\" and url \"") +
         url + std::string("\" and source ") +
         testing::PrintToString(static_cast<int>(source));
}

MATCHER_P3(MatchesTile, title, url, source, PrintTile(title, url, source)) {
  return arg.title == base::ASCIIToUTF16(title) && arg.url == GURL(url) &&
         arg.source == source;
}

std::string PrintTileSource(TileSource source) {
  return std::string("has source ") +
         testing::PrintToString(static_cast<int>(source));
}

MATCHER_P(TileWithSource, source, PrintTileSource(source)) {
  return arg.source == source;
}

MATCHER_P3(LastTileIs,
           title,
           url,
           source,
           std::string("last tile ") + PrintTile(title, url, source)) {
  const NTPTilesVector& tiles = arg.at(SectionType::PERSONALIZED);
  if (tiles.empty())
    return false;

  const NTPTile& last = tiles.back();
  return last.title == base::ASCIIToUTF16(title) && last.url == GURL(url) &&
         last.source == source;
}

MATCHER_P3(FirstPersonalizedTileIs,
           title,
           url,
           source,
           std::string("first tile ") + PrintTile(title, url, source)) {
  if (arg.count(SectionType::PERSONALIZED) == 0) {
    return false;
  }
  const NTPTilesVector& tiles = arg.at(SectionType::PERSONALIZED);
  return !tiles.empty() && tiles[0].title == base::ASCIIToUTF16(title) &&
         tiles[0].url == GURL(url) && tiles[0].source == source;
}

// testing::InvokeArgument<N> does not work with base::Callback, fortunately
// gmock makes it simple to create action templates that do for the various
// possible numbers of arguments.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(std::get<k>(args)).Run(p0);
}

NTPTile MakeTile(const std::string& title,
                 const std::string& url,
                 TileSource source) {
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
  void GetMostVisitedURLs(GetMostVisitedURLsCallback callback) override {
    GetMostVisitedURLs_(callback);
  }
  MOCK_METHOD1(GetMostVisitedURLs_, void(GetMostVisitedURLsCallback& callback));
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
  MOCK_METHOD0(IsFull, bool());
  MOCK_CONST_METHOD0(loaded, bool());
  MOCK_METHOD0(GetPrepopulatedPages, history::PrepopulatedPageList());
  MOCK_METHOD1(OnNavigationCommitted, void(const GURL& url));

  // Publicly expose notification to observers, since the implementation cannot
  // be overriden.
  using TopSites::NotifyTopSitesChanged;

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
  MOCK_METHOD1(BlacklistURL, bool(const GURL& candidate_url));
  MOCK_METHOD1(UndoBlacklistURL, bool(const GURL& url));
  MOCK_METHOD0(ClearBlacklist, void());
};

class MockMostVisitedSitesObserver : public MostVisitedSites::Observer {
 public:
  MOCK_METHOD1(OnURLsAvailable,
               void(const std::map<SectionType, NTPTilesVector>& sections));
  MOCK_METHOD1(OnIconMadeAvailable, void(const GURL& site_url));
};

class FakeHomepageClient : public MostVisitedSites::HomepageClient {
 public:
  FakeHomepageClient()
      : homepage_tile_enabled_(false), homepage_url_(kHomepageUrl) {}
  ~FakeHomepageClient() override {}

  bool IsHomepageTileEnabled() const override { return homepage_tile_enabled_; }

  GURL GetHomepageUrl() const override { return homepage_url_; }

  void QueryHomepageTitle(TitleCallback title_callback) override {
    std::move(title_callback).Run(homepage_title_);
  }

  void SetHomepageTileEnabled(bool homepage_tile_enabled) {
    homepage_tile_enabled_ = homepage_tile_enabled;
  }

  void SetHomepageUrl(GURL homepage_url) { homepage_url_ = homepage_url; }

  void SetHomepageTitle(const base::Optional<base::string16>& homepage_title) {
    homepage_title_ = homepage_title;
  }

 private:
  bool homepage_tile_enabled_;
  GURL homepage_url_;
  base::Optional<base::string16> homepage_title_;
};

class FakeExploreSitesClient : public MostVisitedSites::ExploreSitesClient {
 public:
  ~FakeExploreSitesClient() override = default;

  GURL GetExploreSitesUrl() const override { return GURL(kTestExploreUrl); }

  base::string16 GetExploreSitesTitle() const override {
    return base::ASCIIToUTF16(kTestExploreTitle);
  }
};

class MockIconCacher : public IconCacher {
 public:
  MOCK_METHOD3(StartFetchPopularSites,
               void(PopularSites::Site site,
                    base::OnceClosure icon_available,
                    base::OnceClosure preliminary_icon_available));
  MOCK_METHOD2(StartFetchMostLikely,
               void(const GURL& page_url, base::OnceClosure icon_available));
};

class MockCustomLinksManager : public CustomLinksManager {
 public:
  MOCK_METHOD1(Initialize, bool(const NTPTilesVector& tiles));
  MOCK_METHOD0(Uninitialize, void());
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_CONST_METHOD0(GetLinks, const std::vector<CustomLinksManager::Link>&());
  MOCK_METHOD2(AddLink, bool(const GURL& url, const base::string16& title));
  MOCK_METHOD3(UpdateLink,
               bool(const GURL& url,
                    const GURL& new_url,
                    const base::string16& new_title));
  MOCK_METHOD2(ReorderLink, bool(const GURL& url, size_t new_pos));
  MOCK_METHOD1(DeleteLink, bool(const GURL& url));
  MOCK_METHOD0(UndoAction, bool());
  MOCK_METHOD1(RegisterCallbackForOnChanged,
               std::unique_ptr<base::CallbackList<void()>::Subscription>(
                   base::RepeatingClosure callback));
};

class PopularSitesFactoryForTest {
 public:
  PopularSitesFactoryForTest(
      sync_preferences::TestingPrefServiceSyncable* pref_service)
      : prefs_(pref_service) {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    PopularSitesImpl::RegisterProfilePrefs(pref_service->registry());
  }

  void SeedWithSampleData() {
    prefs_->SetString(prefs::kPopularSitesOverrideCountry, "IN");
    prefs_->SetString(prefs::kPopularSitesOverrideVersion, "5");

    test_url_loader_factory_.ClearResponses();
    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/suggested_sites_IN_5.json",
        R"([{
              "title": "PopularSite1",
              "url": "http://popularsite1/",
              "favicon_url": "http://popularsite1/favicon.ico"
            },
            {
              "title": "PopularSite2",
              "url": "http://popularsite2/",
              "favicon_url": "http://popularsite2/favicon.ico"
            }
           ])");

    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/suggested_sites_US_5.json",
        R"([{
              "title": "ESPN",
              "url": "http://www.espn.com",
              "favicon_url": "http://www.espn.com/favicon.ico"
            }, {
              "title": "Mobile",
              "url": "http://www.mobile.de",
              "favicon_url": "http://www.mobile.de/favicon.ico"
            }, {
              "title": "Google News",
              "url": "http://news.google.com",
              "favicon_url": "http://news.google.com/favicon.ico"
            }
           ])");

    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/suggested_sites_IN_6.json",
        R"([{
              "section": 1, // PERSONALIZED
              "sites": [{
                  "title": "PopularSite1",
                  "url": "http://popularsite1/",
                  "favicon_url": "http://popularsite1/favicon.ico"
                },
                {
                  "title": "PopularSite2",
                  "url": "http://popularsite2/",
                  "favicon_url": "http://popularsite2/favicon.ico"
                },
               ]
            },
            {
                "section": 4,  // NEWS
                "sites": [{
                    "large_icon_url": "https://news.google.com/icon.ico",
                    "title": "Google News",
                    "url": "https://news.google.com/"
                },
                {
                    "favicon_url": "https://news.google.com/icon.ico",
                    "title": "Google News Germany",
                    "url": "https://news.google.de/"
                }]
            },
            {
                "section": 2,  // SOCIAL
                "sites": [{
                    "large_icon_url": "https://ssl.gstatic.com/icon.png",
                    "title": "Google+",
                    "url": "https://plus.google.com/"
                }]
            },
            {
                "section": 3,  // ENTERTAINMENT
                "sites": [
                    // Intentionally empty site list.
                ]
            }
        ])");
  }

  std::unique_ptr<PopularSites> New() {
    return std::make_unique<PopularSitesImpl>(prefs_,
                                              /*template_url_service=*/nullptr,
                                              /*variations_service=*/nullptr,
                                              test_shared_loader_factory_);
  }

 private:
  PrefService* prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

// CallbackList-like container without Subscription, mimicking the
// implementation in TopSites (which doesn't use base::CallbackList).
class TopSitesCallbackList {
 public:
  void Add(TopSites::GetMostVisitedURLsCallback& callback) {
    callbacks_.push_back(std::move(callback));
  }

  void ClearAndNotify(const MostVisitedURLList& list) {
    std::vector<TopSites::GetMostVisitedURLsCallback> callbacks;
    callbacks.swap(callbacks_);
    for (auto& callback : callbacks)
      std::move(callback).Run(list);
  }

  bool empty() const { return callbacks_.empty(); }

 private:
  std::vector<TopSites::GetMostVisitedURLsCallback> callbacks_;
};

}  // namespace

// Param is a tuple with two components:
// * The first specifies whether Popular Sites is enabled via variations.
// * The second specifies whether Suggestions Service tiles are enabled via
//   variations.
class MostVisitedSitesTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  MostVisitedSitesTest()
      : is_custom_links_enabled_(false),
        popular_sites_factory_(&pref_service_),
        mock_top_sites_(new StrictMock<MockTopSites>()) {
    MostVisitedSites::RegisterProfilePrefs(pref_service_.registry());

    std::vector<base::Feature> enabled_features;
    // Disable FaviconServer in most tests and override in specific tests.
    std::vector<base::Feature> disabled_features = {
        kNtpMostLikelyFaviconsFromServerFeature};
    if (IsPopularSitesFeatureEnabled()) {
      enabled_features.push_back(kUsePopularSitesSuggestions);
    } else {
      disabled_features.push_back(kUsePopularSitesSuggestions);
    }
    if (IsDisplaySuggestionsServiceTilesEnabled()) {
      enabled_features.push_back(kDisplaySuggestionsServiceTiles);
    } else {
      disabled_features.push_back(kDisplaySuggestionsServiceTiles);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    if (IsPopularSitesFeatureEnabled())
      popular_sites_factory_.SeedWithSampleData();

    RecreateMostVisitedSites();
  }

  void RecreateMostVisitedSites() {
    // We use StrictMock to make sure the object is not used unless Popular
    // Sites is enabled.
    auto icon_cacher = std::make_unique<StrictMock<MockIconCacher>>();
    icon_cacher_ = icon_cacher.get();

    // Custom links needs to be nullptr when MostVisitedSites is created, unless
    // the custom links feature is enabled. Custom links is disabled for
    // Android, iOS, and third-party NTPs.
    std::unique_ptr<StrictMock<MockCustomLinksManager>> mock_custom_links;
    if (is_custom_links_enabled_) {
      mock_custom_links =
          std::make_unique<StrictMock<MockCustomLinksManager>>();
      mock_custom_links_ = mock_custom_links.get();
    }

    if (IsPopularSitesFeatureEnabled()) {
      // Populate Popular Sites' internal cache by mimicking a past usage of
      // PopularSitesImpl.
      auto tmp_popular_sites = popular_sites_factory_.New();
      base::RunLoop loop;
      bool save_success = false;
      tmp_popular_sites->MaybeStartFetch(
          /*force_download=*/true,
          base::BindOnce(
              [](bool* save_success, base::RunLoop* loop, bool success) {
                *save_success = success;
                loop->Quit();
              },
              &save_success, &loop));
      loop.Run();
      EXPECT_TRUE(save_success);

      // With PopularSites enabled, blacklist is exercised.
      EXPECT_CALL(*mock_top_sites_, IsBlacklisted(_))
          .WillRepeatedly(Return(false));
      // Mock icon cacher never replies, and we also don't verify whether the
      // code uses it correctly.
      EXPECT_CALL(*icon_cacher, StartFetchPopularSites(_, _, _))
          .Times(AtLeast(0));
    }

    EXPECT_CALL(*icon_cacher, StartFetchMostLikely(_, _)).Times(AtLeast(0));

    most_visited_sites_ = std::make_unique<MostVisitedSites>(
        &pref_service_, mock_top_sites_, &mock_suggestions_service_,
        popular_sites_factory_.New(), std::move(mock_custom_links),
        std::move(icon_cacher),
        /*supervisor=*/nullptr);
  }

  bool IsPopularSitesFeatureEnabled() const { return std::get<0>(GetParam()); }
  bool IsDisplaySuggestionsServiceTilesEnabled() const {
    return std::get<1>(GetParam());
  }

  bool VerifyAndClearExpectations() {
    base::RunLoop().RunUntilIdle();
    const bool success =
        Mock::VerifyAndClearExpectations(mock_top_sites_.get()) &&
        Mock::VerifyAndClearExpectations(&mock_suggestions_service_) &&
        Mock::VerifyAndClearExpectations(&mock_observer_);
    // For convenience, restore the expectations for IsBlacklisted().
    if (IsPopularSitesFeatureEnabled()) {
      EXPECT_CALL(*mock_top_sites_, IsBlacklisted(_))
          .WillRepeatedly(Return(false));
    }
    return success;
  }

  FakeHomepageClient* RegisterNewHomepageClient() {
    auto homepage_client = std::make_unique<FakeHomepageClient>();
    FakeHomepageClient* raw_client_ptr = homepage_client.get();
    most_visited_sites_->SetHomepageClient(std::move(homepage_client));
    return raw_client_ptr;
  }

  FakeExploreSitesClient* RegisterNewExploreSitesClient() {
    auto explore_sites_client = std::make_unique<FakeExploreSitesClient>();
    FakeExploreSitesClient* raw_client_ptr = explore_sites_client.get();
    most_visited_sites_->SetExploreSitesClient(std::move(explore_sites_client));
    return raw_client_ptr;
  }

  void DisableRemoteSuggestions() {
    EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&suggestions_service_callbacks_,
                               &SuggestionsService::ResponseCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
        .Times(AnyNumber())
        .WillRepeatedly(Return(SuggestionsProfile()));  // Empty cache.
    EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
  }

  void EnableCustomLinks() { is_custom_links_enabled_ = true; }

  bool is_custom_links_enabled_;
  base::CallbackList<SuggestionsService::ResponseCallback::RunType>
      suggestions_service_callbacks_;
  TopSitesCallbackList top_sites_callbacks_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  PopularSitesFactoryForTest popular_sites_factory_;
  scoped_refptr<StrictMock<MockTopSites>> mock_top_sites_;
  StrictMock<MockSuggestionsService> mock_suggestions_service_;
  StrictMock<MockMostVisitedSitesObserver> mock_observer_;
  std::unique_ptr<MostVisitedSites> most_visited_sites_;
  base::test::ScopedFeatureList feature_list_;
  MockCustomLinksManager* mock_custom_links_;
  MockIconCacher* icon_cacher_;
};

TEST_P(MostVisitedSitesTest, ShouldStartNoCallInConstructor) {
  // No call to mocks expected by the mere fact of instantiating
  // MostVisitedSites.
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldRefreshBothBackends) {
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData());
  most_visited_sites_->Refresh();
}

TEST_P(MostVisitedSitesTest, ShouldIncludeTileForHomepage) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(FirstPersonalizedTileIs(
                                  "", kHomepageUrl, TileSource::HOMEPAGE)));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageWithoutClient) {
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile("", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldIncludeHomeTileWithUrlBeforeQueryingName) {
  // Because the query time for the real name might take a while, provide the
  // home tile with URL as title immediately and update the tiles as soon as the
  // real title was found.
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  homepage_client->SetHomepageTitle(base::UTF8ToUTF16(kHomepageTitle));
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  {
    testing::Sequence seq;
    EXPECT_CALL(mock_observer_,
                OnURLsAvailable(Contains(
                    Pair(SectionType::PERSONALIZED,
                         Not(Contains(MatchesTile("", kHomepageUrl,
                                                  TileSource::HOMEPAGE)))))));
    EXPECT_CALL(mock_observer_,
                OnURLsAvailable(Contains(
                    Pair(SectionType::PERSONALIZED,
                         Not(Contains(MatchesTile(kHomepageTitle, kHomepageUrl,
                                                  TileSource::HOMEPAGE)))))));
  }
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldUpdateHomepageTileWhenRefreshHomepageTile) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();

  // Ensure that home tile is available as usual.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(FirstPersonalizedTileIs(
                                  "", kHomepageUrl, TileSource::HOMEPAGE)));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();

  // Disable home page and rebuild _without_ Resync. The tile should be gone.
  homepage_client->SetHomepageTileEnabled(false);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory()).Times(0);
  EXPECT_CALL(mock_observer_, OnURLsAvailable(Not(FirstPersonalizedTileIs(
                                  "", kHomepageUrl, TileSource::HOMEPAGE))));
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfNoTileRequested) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(SectionType::PERSONALIZED, IsEmpty()))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldReturnHomepageIfOneTileRequested) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          (MostVisitedURLList{MakeMostVisitedURL("Site 1", "http://site1/")})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(MatchesTile("", kHomepageUrl, TileSource::HOMEPAGE))))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldHaveHomepageFirstInListWhenFull) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>((MostVisitedURLList{
          MakeMostVisitedURL("Site 1", "http://site1/"),
          MakeMostVisitedURL("Site 2", "http://site2/"),
          MakeMostVisitedURL("Site 3", "http://site3/"),
          MakeMostVisitedURL("Site 4", "http://site4/"),
          MakeMostVisitedURL("Site 5", "http://site5/"),
      })));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/4);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(4ul));
  // Assert that the home page is appended as the final tile.
  EXPECT_THAT(tiles[0], MatchesTile("", kHomepageUrl, TileSource::HOMEPAGE));
}

TEST_P(MostVisitedSitesTest, ShouldHaveHomepageFirstInListWhenNotFull) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>((MostVisitedURLList{
          MakeMostVisitedURL("Site 1", "http://site1/"),
          MakeMostVisitedURL("Site 2", "http://site2/"),
          MakeMostVisitedURL("Site 3", "http://site3/"),
          MakeMostVisitedURL("Site 4", "http://site4/"),
          MakeMostVisitedURL("Site 5", "http://site5/"),
      })));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/8);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(6ul));
  // Assert that the home page is the first tile.
  EXPECT_THAT(tiles[0], MatchesTile("", kHomepageUrl, TileSource::HOMEPAGE));
}

TEST_P(MostVisitedSitesTest, ShouldDeduplicateHomepageWithTopSites) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          (MostVisitedURLList{MakeMostVisitedURL("Site 1", "http://site1/"),
                              MakeMostVisitedURL("", kHomepageUrl)})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          AllOf(Contains(MatchesTile("", kHomepageUrl, TileSource::HOMEPAGE)),
                Not(Contains(
                    MatchesTile("", kHomepageUrl, TileSource::TOP_SITES))))))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfThereIsNone) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(false);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile("", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfEmptyUrl) {
  const std::string kEmptyHomepageUrl;
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  homepage_client->SetHomepageUrl(GURL(kEmptyHomepageUrl));
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(kEmptyHomepageUrl)))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Not(FirstPersonalizedTileIs(
                  "", kEmptyHomepageUrl, TileSource::HOMEPAGE))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfBlacklisted) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          (MostVisitedURLList{MakeMostVisitedURL("", kHomepageUrl)})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile("", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));

  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldPinHomepageAgainIfBlacklistingUndone) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);

  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillOnce(InvokeCallbackArgument<0>(
          (MostVisitedURLList{MakeMostVisitedURL("", kHomepageUrl)})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile("", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));

  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();

  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillOnce(InvokeCallbackArgument<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, IsBlacklisted(Eq(GURL(kHomepageUrl))))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          Contains(MatchesTile("", kHomepageUrl, TileSource::HOMEPAGE))))));

  most_visited_sites_->OnBlockedSitesChanged();
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeTileForExploreSitesIfNoClient) {
  // Does not register an explore sites client.

  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{
          MakeMostVisitedURL("ESPN", "http://espn.com/"),
          MakeMostVisitedURL("Mobile", "http://m.mobile.de/"),
          MakeMostVisitedURL("Google", "http://www.google.com/")}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Not(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Contains(TileWithSource(TileSource::EXPLORE)))))));
  // Note that 5 sites are requested, this means that there should be the 3 from
  // top sites and two from popular sites.
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/5);
  base::RunLoop().RunUntilIdle();
}

// Tests that the explore sites tile appears when there is a mix of top sites
// and popular sites.
TEST_P(MostVisitedSitesTest, ShouldIncludeTileForExploreSites) {
  RegisterNewExploreSitesClient();
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{
          MakeMostVisitedURL("ESPN", "http://espn.com/"),
          MakeMostVisitedURL("Mobile", "http://m.mobile.de/"),
          MakeMostVisitedURL("Google", "http://www.google.com/")}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(LastTileIs(kTestExploreTitle, kTestExploreUrl,
                                         TileSource::EXPLORE)));
  // Note that 5 sites are requested, this means that there should be the 3 from
  // top sites, one from popular sites, and one explore tile.
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/5);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, RemovesPersonalSiteIfExploreSitesTilePresent) {
  RegisterNewExploreSitesClient();
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{
          MakeMostVisitedURL("ESPN", "http://espn.com/"),
          MakeMostVisitedURL("Mobile", "http://m.mobile.de/"),
          MakeMostVisitedURL("Google", "http://www.google.com/")}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(Pair(
                  SectionType::PERSONALIZED,
                  ElementsAre(MatchesTile("ESPN", "http://espn.com/",
                                          TileSource::TOP_SITES),
                              MatchesTile("Mobile", "http://m.mobile.de/",
                                          TileSource::TOP_SITES),
                              MatchesTile(kTestExploreTitle, kTestExploreUrl,
                                          TileSource::EXPLORE))))));
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldInformSuggestionSourcesWhenBlacklisting) {
  EXPECT_CALL(*mock_top_sites_, AddBlacklistedURL(Eq(GURL(kHomepageUrl))))
      .Times(1);
  EXPECT_CALL(mock_suggestions_service_, BlacklistURL(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber());
  most_visited_sites_->AddOrRemoveBlacklistedUrl(GURL(kHomepageUrl),
                                                 /*add_url=*/true);
  EXPECT_CALL(*mock_top_sites_, RemoveBlacklistedURL(Eq(GURL(kHomepageUrl))))
      .Times(1);
  EXPECT_CALL(mock_suggestions_service_,
              UndoBlacklistURL(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber());
  most_visited_sites_->AddOrRemoveBlacklistedUrl(GURL(kHomepageUrl),
                                                 /*add_url=*/false);
}

TEST_P(MostVisitedSitesTest,
       ShouldDeduplicatePopularSitesWithMostVisitedIffHostAndTitleMatches) {
  pref_service_.SetString(prefs::kPopularSitesOverrideCountry, "US");
  RecreateMostVisitedSites();  // Refills cache with ESPN and Google News.
  DisableRemoteSuggestions();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(MostVisitedURLList{
          MakeMostVisitedURL("ESPN", "http://espn.com/"),
          MakeMostVisitedURL("Mobile", "http://m.mobile.de/"),
          MakeMostVisitedURL("Google", "http://www.google.com/")}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));

  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/6);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              Contains(MatchesTile("Google", "http://www.google.com/",
                                   TileSource::TOP_SITES)));
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
                Contains(MatchesTile("Google News", "http://news.google.com/",
                                     TileSource::POPULAR)));
  }
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              AllOf(Contains(MatchesTile("ESPN", "http://espn.com/",
                                         TileSource::TOP_SITES)),
                    Contains(MatchesTile("Mobile", "http://m.mobile.de/",
                                         TileSource::TOP_SITES)),
                    Not(Contains(MatchesTile("ESPN", "http://www.espn.com/",
                                             TileSource::POPULAR))),
                    Not(Contains(MatchesTile("Mobile", "http://www.mobile.de/",
                                             TileSource::POPULAR)))));
}

TEST_P(MostVisitedSitesTest, ShouldHandleTopSitesCacheHit) {
  // If cached, TopSites returns the tiles synchronously, running the callback
  // even before the function returns.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          MostVisitedURLList{MakeMostVisitedURL("Site 1", "http://site1/")}));

  InSequence seq;
  EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
      .WillOnce(Invoke(&suggestions_service_callbacks_,
                       &SuggestionsService::ResponseCallbackList::Add));
  EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
      .WillOnce(Return(SuggestionsProfile()));  // Empty cache.
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(Pair(
            SectionType::PERSONALIZED,
            ElementsAre(
                MatchesTile("Site 1", "http://site1/", TileSource::TOP_SITES),
                MatchesTile("PopularSite1", "http://popularsite1/",
                            TileSource::POPULAR),
                MatchesTile("PopularSite2", "http://popularsite2/",
                            TileSource::POPULAR))))));
  } else {
    EXPECT_CALL(mock_observer_,
                OnURLsAvailable(Contains(
                    Pair(SectionType::PERSONALIZED,
                         ElementsAre(MatchesTile("Site 1", "http://site1/",
                                                 TileSource::TOP_SITES))))));
  }
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
      .WillOnce(Return(true));

  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  VerifyAndClearExpectations();
  EXPECT_FALSE(suggestions_service_callbacks_.empty());
  CHECK(top_sites_callbacks_.empty());

  // Update by TopSites is propagated.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillOnce(InvokeCallbackArgument<0>(
          MostVisitedURLList{MakeMostVisitedURL("Site 2", "http://site2/")}));
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(*mock_top_sites_, IsBlacklisted(_))
        .WillRepeatedly(Return(false));
  }
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_));
  mock_top_sites_->NotifyTopSitesChanged(
      history::TopSitesObserver::ChangeReason::MOST_VISITED);
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(MostVisitedSitesTest,
                         MostVisitedSitesTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST(MostVisitedSitesTest, ShouldDeduplicateDomainWithNoWwwDomain) {
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"www.mobile.de"},
                                                        "mobile.de"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"mobile.de"},
                                                        "www.mobile.de"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"mobile.co.uk"},
                                                        "www.mobile.co.uk"));
}

TEST(MostVisitedSitesTest, ShouldDeduplicateDomainByRemovingMobilePrefixes) {
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"bbc.co.uk"}, "m.bbc.co.uk"));
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"m.bbc.co.uk"}, "bbc.co.uk"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"cnn.com"},
                                                        "edition.cnn.com"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"edition.cnn.com"},
                                                        "cnn.com"));
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"cnn.com"}, "mobile.cnn.com"));
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"mobile.cnn.com"}, "cnn.com"));
}

TEST(MostVisitedSitesTest, ShouldDeduplicateDomainByReplacingMobilePrefixes) {
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"www.bbc.co.uk"},
                                                        "m.bbc.co.uk"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"m.mobile.de"},
                                                        "www.mobile.de"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"www.cnn.com"},
                                                        "edition.cnn.com"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"mobile.cnn.com"},
                                                        "www.cnn.com"));
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
class MostVisitedSitesWithCustomLinksTest : public MostVisitedSitesTest {
 public:
  MostVisitedSitesWithCustomLinksTest() {
    EnableCustomLinks();
    RecreateMostVisitedSites();
  }

  void ExpectBuildWithTopSites(
      const MostVisitedURLList& expected_list,
      std::map<SectionType, NTPTilesVector>* sections) {
    EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
        .WillRepeatedly(InvokeCallbackArgument<0>(expected_list));
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(*mock_custom_links_, IsInitialized())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
        .WillOnce(SaveArg<0>(sections));
  }

  void ExpectBuildWithSuggestions(
      const std::vector<ChromeSuggestion>& suggestions,
      std::map<SectionType, NTPTilesVector>* sections) {
    EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
        .WillOnce(Invoke(&suggestions_service_callbacks_,
                         &SuggestionsService::ResponseCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
        .WillOnce(Return(MakeProfile(suggestions)));
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_custom_links_, IsInitialized())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
        .WillOnce(SaveArg<0>(sections));
  }

  void ExpectBuildWithCustomLinks(
      const std::vector<CustomLinksManager::Link>& expected_links,
      std::map<SectionType, NTPTilesVector>* sections) {
    EXPECT_CALL(*mock_custom_links_, IsInitialized())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_custom_links_, GetLinks())
        .WillOnce(ReturnRef(expected_links));
    EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
        .WillOnce(SaveArg<0>(sections));
  }
};

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldOnlyBuildCustomLinksWhenInitialized) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build tiles when custom links is not initialized. Tiles should be Top
  // Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));

  // Initialize custom links and rebuild tiles. Tiles should be custom links.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->InitializeCustomLinks();
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Uninitialize custom links and rebuild tiles. Tiles should be Top Sites.
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->UninitializeCustomLinks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldFavorCustomLinksOverTopSites) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build tiles when custom links is not initialized. Tiles should be Top
  // Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));

  // Initialize custom links and rebuild tiles. Tiles should be custom links.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->InitializeCustomLinks();
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Initiate notification for new Top Sites. This should be ignored.
  EXPECT_CALL(*mock_custom_links_, IsInitialized()).WillOnce(Return(true));
  // Notify with an empty SuggestionsProfile first to trigger a query for
  // TopSites.
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 2", "http://site2/")});
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldFavorCustomLinksOverSuggestions) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is not initialized. Tiles should be from
  // suggestions.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithSuggestions({MakeSuggestion(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle, kTestUrl,
                                      TileSource::SUGGESTIONS_SERVICE)));

  // Initialize custom links and rebuild tiles. Tiles should be custom links.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->InitializeCustomLinks();
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Initiate notification for new suggestions. This should be ignored.
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 2", "http://site2/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       DisableCustomLinksWhenNotInitialized) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is not initialized. Tiles should be from
  // suggestions.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithSuggestions({MakeSuggestion(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle, kTestUrl,
                                      TileSource::SUGGESTIONS_SERVICE)));

  // Disable custom links. Tiles should rebuild but not send an update.
  EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
      .WillOnce(Return(MakeProfile({MakeSuggestion(kTestTitle, kTestUrl)})));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  most_visited_sites_->EnableCustomLinks(false);
  base::RunLoop().RunUntilIdle();

  // Try to disable custom links again. This should not rebuild the tiles.
  EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
      .Times(0);
  EXPECT_CALL(*mock_custom_links_, GetLinks()).Times(0);
  most_visited_sites_->EnableCustomLinks(false);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest, DisableCustomLinksWhenInitialized) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is initialized and not disabled. Tiles should
  // be custom links.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
      .WillOnce(Invoke(&suggestions_service_callbacks_,
                       &SuggestionsService::ResponseCallbackList::Add));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
      .WillOnce(Return(false));
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Disable custom links. Tiles should rebuild and return suggestions.
  EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
      .WillOnce(Return(MakeProfile({MakeSuggestion(kTestTitle, kTestUrl)})));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->EnableCustomLinks(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle, kTestUrl,
                                      TileSource::SUGGESTIONS_SERVICE)));

  // Re-enable custom links. Tiles should rebuild and return custom links.
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->EnableCustomLinks(true);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldGenerateShortTitleForTopSites) {
  std::string kTestUrl1 = "https://www.imdb.com/";
  std::string kTestTitle1 = "IMDb - Movies, TV and Celebrities - IMDb";
  std::string kTestUrl2 = "https://drive.google.com/";
  std::string kTestTitle2 =
      "Google Drive - Cloud Storage & File Backup for Photos, Docs & More";
  std::string kTestUrl3 = "https://amazon.com/";
  std::string kTestTitle3 =
      "Amazon.com: Online Shopping for Electronics, Apparel, Computers, Books, "
      "DVDs & more";
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build tiles from Top Sites. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1),
                         MakeMostVisitedURL(kTestTitle2, kTestUrl2),
                         MakeMostVisitedURL(kTestTitle3, kTestUrl3)},
      &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(
          MatchesTile(/* The short title generated by the heuristic */ "IMDb",
                      kTestUrl1, TileSource::TOP_SITES),
          MatchesTile(
              /* The short title generated by the heuristic */ "Google Drive",
              kTestUrl2, TileSource::TOP_SITES),
          MatchesTile(
              /* The short title generated by the heuristic */ "Amazon.com",
              kTestUrl3, TileSource::TOP_SITES)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldGenerateShortTitleForSuggestions) {
  std::string kTestUrl1 = "https://www.imdb.com/";
  std::string kTestTitle1 = "IMDb - Movies, TV and Celebrities - IMDb";
  std::string kTestUrl2 = "https://drive.google.com/";
  std::string kTestTitle2 =
      "Google Drive - Cloud Storage & File Backup for Photos, Docs & More";
  std::string kTestUrl3 = "https://amazon.com/";
  std::string kTestTitle3 =
      "Amazon.com: Online Shopping for Electronics, Apparel, Computers, Books, "
      "DVDs & more";
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles from Suggestions. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithSuggestions({MakeSuggestion(kTestTitle1, kTestUrl1),
                              MakeSuggestion(kTestTitle2, kTestUrl2),
                              MakeSuggestion(kTestTitle3, kTestUrl3)},
                             &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/3);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(
          MatchesTile(/* The short title generated by the heuristic */ "IMDb",
                      kTestUrl1, TileSource::SUGGESTIONS_SERVICE),
          MatchesTile(
              /* The short title generated by the heuristic */ "Google Drive",
              kTestUrl2, TileSource::SUGGESTIONS_SERVICE),
          MatchesTile(
              /* The short title generated by the heuristic */ "Amazon.com",
              kTestUrl3, TileSource::SUGGESTIONS_SERVICE)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldNotCrashIfReceiveAnEmptyTitle) {
  std::string kTestUrl1 = "https://site1/";
  std::string kTestTitle1 = "";  // Empty title
  std::string kTestUrl2 = "https://site2/";
  std::string kTestTitle2 = "       ";  // Title only contains spaces
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build tiles from Top Sites. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1),
                         MakeMostVisitedURL(kTestTitle2, kTestUrl2)},
      &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/2);
  base::RunLoop().RunUntilIdle();

  // Both cases should not crash and generate an empty title tile.
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile("", kTestUrl1, TileSource::TOP_SITES),
                          MatchesTile("", kTestUrl2, TileSource::TOP_SITES)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       UninitializeCustomLinksOnUndoAfterFirstAction) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build initial tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));

  // Initialize custom links and complete a custom link action.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, AddLink(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillRepeatedly(ReturnRef(expected_links));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->AddCustomLink(GURL("test.com"),
                                     base::UTF8ToUTF16("test"));
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Undo the action. This should uninitialize custom links.
  EXPECT_CALL(*mock_custom_links_, UndoAction()).Times(0);
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->UndoCustomLinkAction();
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       DontUninitializeCustomLinksOnUndoAfterMultipleActions) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build initial tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));

  // Initialize custom links and complete a custom link action.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, UpdateLink(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillRepeatedly(ReturnRef(expected_links));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->UpdateCustomLink(GURL("test.com"), GURL("test.com"),
                                        base::UTF8ToUTF16("test"));
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Complete a second custom link action.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, DeleteLink(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillOnce(ReturnRef(expected_links));
  most_visited_sites_->DeleteCustomLink(GURL("test.com"));
  base::RunLoop().RunUntilIdle();

  // Undo the second action. This should not uninitialize custom links.
  EXPECT_CALL(*mock_custom_links_, UndoAction()).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, Uninitialize()).Times(0);
  EXPECT_CALL(*mock_custom_links_, IsInitialized()).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillOnce(ReturnRef(expected_links));
  most_visited_sites_->UndoCustomLinkAction();
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       UninitializeCustomLinksIfFirstActionFails) {
  const char kTestUrl[] = "http://site1/";
  const char kTestTitle[] = "Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl),
                                base::UTF8ToUTF16(kTestTitle)}});
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build initial tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES)));

  // Fail to add a custom link. This should not initialize custom links nor
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, AddLink(_, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  most_visited_sites_->AddCustomLink(GURL(kTestUrl), base::UTF8ToUTF16("test"));
  base::RunLoop().RunUntilIdle();

  // Fail to edit a custom link. This should not initialize custom links nor
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, UpdateLink(_, _, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  most_visited_sites_->UpdateCustomLink(GURL("test.com"), GURL("test2.com"),
                                        base::UTF8ToUTF16("test"));
  base::RunLoop().RunUntilIdle();

  // Fail to reorder a custom link. This should not initialize custom links nor
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, ReorderLink(_, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  most_visited_sites_->ReorderCustomLink(GURL("test.com"), 1);
  base::RunLoop().RunUntilIdle();

  // Fail to delete a custom link. This should not initialize custom links nor
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, DeleteLink(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  most_visited_sites_->DeleteCustomLink(GURL("test.com"));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest, RebuildTilesOnCustomLinksChanged) {
  const char kTestUrl1[] = "http://site1/";
  const char kTestUrl2[] = "http://site2/";
  const char kTestTitle1[] = "Site 1";
  const char kTestTitle2[] = "Site 2";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl2),
                                base::UTF8ToUTF16(kTestTitle2)}});
  std::map<SectionType, NTPTilesVector> sections;
  DisableRemoteSuggestions();

  // Build initial tiles with Top Sites.
  base::RepeatingClosure custom_links_callback;
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_))
      .WillOnce(
          DoAll(SaveArg<0>(&custom_links_callback), Return(ByMove(nullptr))));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1)},
      &sections);
  most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                  /*num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle1, kTestUrl1, TileSource::TOP_SITES)));

  // Notify that there is a new set of custom links. This should replace the
  // current tiles with custom links.
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillRepeatedly(ReturnRef(expected_links));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  custom_links_callback.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle2, kTestUrl2,
                                      TileSource::CUSTOM_LINKS)));

  // Notify that custom links have been uninitialized. This should rebuild the
  // tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillRepeatedly(InvokeCallbackArgument<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  custom_links_callback.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle1, kTestUrl1, TileSource::TOP_SITES)));
}

// These tests expect Most Likely to be enabled, and exclude Android and iOS,
// so this will continue to be the case.
INSTANTIATE_TEST_SUITE_P(MostVisitedSitesWithCustomLinksTest,
                         MostVisitedSitesWithCustomLinksTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values(true)));
#endif

class MostVisitedSitesWithCacheHitTest : public MostVisitedSitesTest {
 public:
  // Constructor sets the common expectations for the case where suggestions
  // service has cached results when the observer is registered.
  MostVisitedSitesWithCacheHitTest() {
    InSequence seq;
    EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
        .WillOnce(Invoke(&suggestions_service_callbacks_,
                         &SuggestionsService::ResponseCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
        .WillOnce(Return(MakeProfile({
            MakeSuggestion("Site 1", "http://site1/"),
            MakeSuggestion("Site 2", "http://site2/"),
            MakeSuggestion("Site 3", "http://site3/"),
        })));

    if (IsPopularSitesFeatureEnabled()) {
      EXPECT_CALL(
          mock_observer_,
          OnURLsAvailable(Contains(Pair(
              SectionType::PERSONALIZED,
              ElementsAre(MatchesTile("Site 1", "http://site1/",
                                      TileSource::SUGGESTIONS_SERVICE),
                          MatchesTile("Site 2", "http://site2/",
                                      TileSource::SUGGESTIONS_SERVICE),
                          MatchesTile("Site 3", "http://site3/",
                                      TileSource::SUGGESTIONS_SERVICE),
                          MatchesTile("PopularSite1", "http://popularsite1/",
                                      TileSource::POPULAR))))));
    } else {
      EXPECT_CALL(
          mock_observer_,
          OnURLsAvailable(Contains(Pair(
              SectionType::PERSONALIZED,
              ElementsAre(MatchesTile("Site 1", "http://site1/",
                                      TileSource::SUGGESTIONS_SERVICE),
                          MatchesTile("Site 2", "http://site2/",
                                      TileSource::SUGGESTIONS_SERVICE),
                          MatchesTile("Site 3", "http://site3/",
                                      TileSource::SUGGESTIONS_SERVICE))))));
    }
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
        .WillOnce(Return(true));

    most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                    /*num_sites=*/4);
    VerifyAndClearExpectations();

    EXPECT_FALSE(suggestions_service_callbacks_.empty());
    EXPECT_TRUE(top_sites_callbacks_.empty());
  }
};

TEST_P(MostVisitedSitesWithCacheHitTest, ShouldFavorSuggestionsServiceCache) {
  // Constructor sets basic expectations for a suggestions service cache hit.
}

TEST_P(MostVisitedSitesWithCacheHitTest,
       ShouldPropagateUpdateBySuggestionsService) {
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(Pair(
                  SectionType::PERSONALIZED,
                  ElementsAre(MatchesTile("Site 4", "http://site4/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 5", "http://site5/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 6", "http://site6/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 7", "http://site7/",
                                          TileSource::SUGGESTIONS_SERVICE))))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 4", "http://site4/"),
                   MakeSuggestion("Site 5", "http://site5/"),
                   MakeSuggestion("Site 6", "http://site6/"),
                   MakeSuggestion("Site 7", "http://site7/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCacheHitTest, ShouldTruncateList) {
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(SectionType::PERSONALIZED, SizeIs(4)))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 4", "http://site4/"),
                   MakeSuggestion("Site 5", "http://site5/"),
                   MakeSuggestion("Site 6", "http://site6/"),
                   MakeSuggestion("Site 7", "http://site7/"),
                   MakeSuggestion("Site 8", "http://site8/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCacheHitTest,
       ShouldCompleteWithPopularSitesIffEnabled) {
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(
            Pair(SectionType::PERSONALIZED,
                 ElementsAre(MatchesTile("Site 4", "http://site4/",
                                         TileSource::SUGGESTIONS_SERVICE),
                             MatchesTile("PopularSite1", "http://popularsite1/",
                                         TileSource::POPULAR),
                             MatchesTile("PopularSite2", "http://popularsite2/",
                                         TileSource::POPULAR))))));
  } else {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(
            Pair(SectionType::PERSONALIZED,
                 ElementsAre(MatchesTile("Site 4", "http://site4/",
                                         TileSource::SUGGESTIONS_SERVICE))))));
  }
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 4", "http://site4/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCacheHitTest,
       ShouldSwitchToTopSitesIfEmptyUpdateBySuggestionsService) {
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();

  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 4", "http://site4/", TileSource::TOP_SITES),
              MatchesTile("Site 5", "http://site5/", TileSource::TOP_SITES),
              MatchesTile("Site 6", "http://site6/", TileSource::TOP_SITES),
              MatchesTile("Site 7", "http://site7/",
                          TileSource::TOP_SITES))))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 4", "http://site4/"),
       MakeMostVisitedURL("Site 5", "http://site5/"),
       MakeMostVisitedURL("Site 6", "http://site6/"),
       MakeMostVisitedURL("Site 7", "http://site7/")});
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCacheHitTest, ShouldFetchFaviconsIfEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNtpMostLikelyFaviconsFromServerFeature);

  EXPECT_CALL(mock_observer_, OnURLsAvailable(_));
  EXPECT_CALL(*icon_cacher_, StartFetchMostLikely(GURL("http://site4/"), _));

  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 4", "http://site4/")}));
  base::RunLoop().RunUntilIdle();
}

// Tests only apply when the suggestions service is enabled.
INSTANTIATE_TEST_SUITE_P(MostVisitedSitesWithCacheHitTest,
                         MostVisitedSitesWithCacheHitTest,
                         ::testing::Combine(::testing::Bool(),
                                            testing::Values(true)));

class MostVisitedSitesWithEmptyCacheTest : public MostVisitedSitesTest {
 public:
  // Constructor sets the common expectations for the case where suggestions
  // service doesn't have cached results when the observer is registered.
  MostVisitedSitesWithEmptyCacheTest() {
    InSequence seq;
    EXPECT_CALL(mock_suggestions_service_, AddCallback(_))
        .WillOnce(Invoke(&suggestions_service_callbacks_,
                         &SuggestionsService::ResponseCallbackList::Add));
    EXPECT_CALL(mock_suggestions_service_, GetSuggestionsDataFromCache())
        .WillOnce(Return(SuggestionsProfile()));  // Empty cache.
    EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
        .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(mock_suggestions_service_, FetchSuggestionsData())
        .WillOnce(Return(true));

    most_visited_sites_->SetMostVisitedURLsObserver(&mock_observer_,
                                                    /*num_sites=*/3);
    VerifyAndClearExpectations();

    EXPECT_FALSE(suggestions_service_callbacks_.empty());
    EXPECT_FALSE(top_sites_callbacks_.empty());
  }
};

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldQueryTopSitesAndSuggestionsService) {
  // Constructor sets basic expectations for a suggestions service cache miss.
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldCompleteWithPopularSitesIffEnabled) {
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(
            Pair(SectionType::PERSONALIZED,
                 ElementsAre(MatchesTile("Site 4", "http://site4/",
                                         TileSource::SUGGESTIONS_SERVICE),
                             MatchesTile("PopularSite1", "http://popularsite1/",
                                         TileSource::POPULAR),
                             MatchesTile("PopularSite2", "http://popularsite2/",
                                         TileSource::POPULAR))))));
  } else {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(
            Pair(SectionType::PERSONALIZED,
                 ElementsAre(MatchesTile("Site 4", "http://site4/",
                                         TileSource::SUGGESTIONS_SERVICE))))));
  }
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 4", "http://site4/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldIgnoreTopSitesIfSuggestionsServiceFaster) {
  // Reply from suggestions service triggers and update to our observer.
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(Pair(
                  SectionType::PERSONALIZED,
                  ElementsAre(MatchesTile("Site 1", "http://site1/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 2", "http://site2/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 3", "http://site3/",
                                          TileSource::SUGGESTIONS_SERVICE))))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 1", "http://site1/"),
                   MakeSuggestion("Site 2", "http://site2/"),
                   MakeSuggestion("Site 3", "http://site3/")}));
  VerifyAndClearExpectations();

  // Reply from top sites is ignored (i.e. not reported to observer).
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 4", "http://site4/")});
  VerifyAndClearExpectations();

  // Update by TopSites is also ignored.
  mock_top_sites_->NotifyTopSitesChanged(
      history::TopSitesObserver::ChangeReason::MOST_VISITED);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldExposeTopSitesIfSuggestionsServiceFasterButEmpty) {
  // Empty reply from suggestions service causes no update to our observer.
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();

  // Reply from top sites is propagated to observer.
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 1", "http://site1/", TileSource::TOP_SITES),
              MatchesTile("Site 2", "http://site2/", TileSource::TOP_SITES),
              MatchesTile("Site 3", "http://site3/",
                          TileSource::TOP_SITES))))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/"),
       MakeMostVisitedURL("Site 2", "http://site2/"),
       MakeMostVisitedURL("Site 3", "http://site3/")});
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldFavorSuggestionsServiceAlthoughSlower) {
  // Reply from top sites is propagated to observer.
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 1", "http://site1/", TileSource::TOP_SITES),
              MatchesTile("Site 2", "http://site2/", TileSource::TOP_SITES),
              MatchesTile("Site 3", "http://site3/",
                          TileSource::TOP_SITES))))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/"),
       MakeMostVisitedURL("Site 2", "http://site2/"),
       MakeMostVisitedURL("Site 3", "http://site3/")});
  VerifyAndClearExpectations();

  // Reply from suggestions service overrides top sites.
  InSequence seq;
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(Pair(
                  SectionType::PERSONALIZED,
                  ElementsAre(MatchesTile("Site 4", "http://site4/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 5", "http://site5/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 6", "http://site6/",
                                          TileSource::SUGGESTIONS_SERVICE))))));
  suggestions_service_callbacks_.Notify(
      MakeProfile({MakeSuggestion("Site 4", "http://site4/"),
                   MakeSuggestion("Site 5", "http://site5/"),
                   MakeSuggestion("Site 6", "http://site6/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldIgnoreSuggestionsServiceIfSlowerAndEmpty) {
  // Reply from top sites is propagated to observer.
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 1", "http://site1/", TileSource::TOP_SITES),
              MatchesTile("Site 2", "http://site2/", TileSource::TOP_SITES),
              MatchesTile("Site 3", "http://site3/",
                          TileSource::TOP_SITES))))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/"),
       MakeMostVisitedURL("Site 2", "http://site2/"),
       MakeMostVisitedURL("Site 3", "http://site3/")});
  VerifyAndClearExpectations();

  // Reply from suggestions service is empty and thus ignored.
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest, ShouldPropagateUpdateByTopSites) {
  // Reply from top sites is propagated to observer.
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 1", "http://site1/", TileSource::TOP_SITES),
              MatchesTile("Site 2", "http://site2/", TileSource::TOP_SITES),
              MatchesTile("Site 3", "http://site3/",
                          TileSource::TOP_SITES))))));
  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/"),
       MakeMostVisitedURL("Site 2", "http://site2/"),
       MakeMostVisitedURL("Site 3", "http://site3/")});
  VerifyAndClearExpectations();

  // Reply from suggestions service is empty and thus ignored.
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  VerifyAndClearExpectations();
  EXPECT_TRUE(top_sites_callbacks_.empty());

  // Update from top sites is propagated to observer.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
      .WillOnce(InvokeCallbackArgument<0>(
          MostVisitedURLList{MakeMostVisitedURL("Site 4", "http://site4/"),
                             MakeMostVisitedURL("Site 5", "http://site5/"),
                             MakeMostVisitedURL("Site 6", "http://site6/")}));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 4", "http://site4/", TileSource::TOP_SITES),
              MatchesTile("Site 5", "http://site5/", TileSource::TOP_SITES),
              MatchesTile("Site 6", "http://site6/",
                          TileSource::TOP_SITES))))));
  mock_top_sites_->NotifyTopSitesChanged(
      history::TopSitesObserver::ChangeReason::MOST_VISITED);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldSendEmptyListIfBothTopSitesAndSuggestionsServiceEmpty) {
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(
            Pair(SectionType::PERSONALIZED,
                 ElementsAre(MatchesTile("PopularSite1", "http://popularsite1/",
                                         TileSource::POPULAR),
                             MatchesTile("PopularSite2", "http://popularsite2/",
                                         TileSource::POPULAR))))));
  } else {
    // The Android NTP doesn't finish initialization until it gets tiles, so a
    // 0-tile notification is always needed.
    EXPECT_CALL(mock_observer_, OnURLsAvailable(ElementsAre(Pair(
                                    SectionType::PERSONALIZED, IsEmpty()))));
  }
  suggestions_service_callbacks_.Notify(SuggestionsProfile());
  top_sites_callbacks_.ClearAndNotify(MostVisitedURLList{});

  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldNotifyOnceIfTopSitesUnchanged) {
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(
              MatchesTile("Site 1", "http://site1/", TileSource::TOP_SITES),
              MatchesTile("Site 2", "http://site2/", TileSource::TOP_SITES),
              MatchesTile("Site 3", "http://site3/",
                          TileSource::TOP_SITES))))));

  suggestions_service_callbacks_.Notify(SuggestionsProfile());

  top_sites_callbacks_.ClearAndNotify(
      {MakeMostVisitedURL("Site 1", "http://site1/"),
       MakeMostVisitedURL("Site 2", "http://site2/"),
       MakeMostVisitedURL("Site 3", "http://site3/")});
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs_(_))
        .WillOnce(Invoke(&top_sites_callbacks_, &TopSitesCallbackList::Add));
    mock_top_sites_->NotifyTopSitesChanged(
        history::TopSitesObserver::ChangeReason::MOST_VISITED);
    EXPECT_FALSE(top_sites_callbacks_.empty());
    top_sites_callbacks_.ClearAndNotify(
        {MakeMostVisitedURL("Site 1", "http://site1/"),
         MakeMostVisitedURL("Site 2", "http://site2/"),
         MakeMostVisitedURL("Site 3", "http://site3/")});
    base::RunLoop().RunUntilIdle();
  }
}

TEST_P(MostVisitedSitesWithEmptyCacheTest,
       ShouldNotifyOnceIfSuggestionsUnchanged) {
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(Pair(
                  SectionType::PERSONALIZED,
                  ElementsAre(MatchesTile("Site 1", "http://site1/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 2", "http://site2/",
                                          TileSource::SUGGESTIONS_SERVICE),
                              MatchesTile("Site 3", "http://site3/",
                                          TileSource::SUGGESTIONS_SERVICE))))));

  for (int i = 0; i < 5; ++i) {
    suggestions_service_callbacks_.Notify(
        MakeProfile({MakeSuggestion("Site 1", "http://site1/"),
                     MakeSuggestion("Site 2", "http://site2/"),
                     MakeSuggestion("Site 3", "http://site3/")}));
  }
}

// Tests only apply when the suggestions service is enabled.
INSTANTIATE_TEST_SUITE_P(MostVisitedSitesWithEmptyCacheTest,
                         MostVisitedSitesWithEmptyCacheTest,
                         ::testing::Combine(::testing::Bool(),
                                            testing::Values(true)));

// This a test for MostVisitedSites::MergeTiles(...) method, and thus has the
// same scope as the method itself. This tests merging popular sites with
// personal tiles.
// More important things out of the scope of testing presently:
// - Removing blacklisted tiles.
// - Correct host extraction from the URL.
// - Ensuring personal tiles are not duplicated in popular tiles.
TEST(MostVisitedSitesMergeTest, ShouldMergeTilesWithPersonalOnly) {
  std::vector<NTPTile> personal_tiles{
      MakeTile("Site 1", "https://www.site1.com/", TileSource::TOP_SITES),
      MakeTile("Site 2", "https://www.site2.com/", TileSource::TOP_SITES),
      MakeTile("Site 3", "https://www.site3.com/", TileSource::TOP_SITES),
      MakeTile("Site 4", "https://www.site4.com/", TileSource::TOP_SITES),
  };
  // Without any popular tiles, the result after merge should be the personal
  // tiles.
  EXPECT_THAT(MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                           /*whitelist_tiles=*/NTPTilesVector(),
                                           /*popular_tiles=*/NTPTilesVector(),
                                           /*explore_tile=*/base::nullopt),
              ElementsAre(MatchesTile("Site 1", "https://www.site1.com/",
                                      TileSource::TOP_SITES),
                          MatchesTile("Site 2", "https://www.site2.com/",
                                      TileSource::TOP_SITES),
                          MatchesTile("Site 3", "https://www.site3.com/",
                                      TileSource::TOP_SITES),
                          MatchesTile("Site 4", "https://www.site4.com/",
                                      TileSource::TOP_SITES)));
}

TEST(MostVisitedSitesMergeTest, ShouldMergeTilesWithPopularOnly) {
  std::vector<NTPTile> popular_tiles{
      MakeTile("Site 1", "https://www.site1.com/", TileSource::POPULAR),
      MakeTile("Site 2", "https://www.site2.com/", TileSource::POPULAR),
      MakeTile("Site 3", "https://www.site3.com/", TileSource::POPULAR),
      MakeTile("Site 4", "https://www.site4.com/", TileSource::POPULAR),
  };
  // Without any personal tiles, the result after merge should be the popular
  // tiles.
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(/*personal_tiles=*/NTPTilesVector(),
                                   /*whitelist_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles),
                                   /*explore_tile=*/base::nullopt),
      ElementsAre(
          MatchesTile("Site 1", "https://www.site1.com/", TileSource::POPULAR),
          MatchesTile("Site 2", "https://www.site2.com/", TileSource::POPULAR),
          MatchesTile("Site 3", "https://www.site3.com/", TileSource::POPULAR),
          MatchesTile("Site 4", "https://www.site4.com/",
                      TileSource::POPULAR)));
}

TEST(MostVisitedSitesMergeTest, ShouldMergeTilesFavoringPersonalOverPopular) {
  std::vector<NTPTile> popular_tiles{
      MakeTile("Site 1", "https://www.site1.com/", TileSource::POPULAR),
      MakeTile("Site 2", "https://www.site2.com/", TileSource::POPULAR),
  };
  std::vector<NTPTile> personal_tiles{
      MakeTile("Site 3", "https://www.site3.com/", TileSource::TOP_SITES),
      MakeTile("Site 4", "https://www.site4.com/", TileSource::TOP_SITES),
  };
  base::Optional<NTPTile> explore_tile{
      MakeTile("Explore", "https://explore.example.com/", TileSource::EXPLORE),
  };
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                   /*whitelist_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles),
                                   /*explore_tiles=*/explore_tile),
      ElementsAre(
          MatchesTile("Site 3", "https://www.site3.com/",
                      TileSource::TOP_SITES),
          MatchesTile("Site 4", "https://www.site4.com/",
                      TileSource::TOP_SITES),
          MatchesTile("Site 1", "https://www.site1.com/", TileSource::POPULAR),
          MatchesTile("Site 2", "https://www.site2.com/", TileSource::POPULAR),
          MatchesTile("Explore", "https://explore.example.com/",
                      TileSource::EXPLORE)));
}

}  // namespace ntp_tiles
