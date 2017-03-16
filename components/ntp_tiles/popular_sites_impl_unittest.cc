// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites_impl.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/json_unsafe_parser.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::Gt;
using testing::IsEmpty;

namespace ntp_tiles {
namespace {

const char kTitle[] = "title";
const char kUrl[] = "url";
const char kLargeIconUrl[] = "large_icon_url";
const char kFaviconUrl[] = "favicon_url";

using TestPopularSite = std::map<std::string, std::string>;
using TestPopularSiteVector = std::vector<TestPopularSite>;

::testing::Matcher<const base::string16&> Str16Eq(const std::string& s) {
  return ::testing::Eq(base::UTF8ToUTF16(s));
}

::testing::Matcher<const GURL&> URLEq(const std::string& s) {
  return ::testing::Eq(GURL(s));
}

size_t GetNumberOfDefaultPopularSitesForPlatform() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return 8ul;
#else
  return 0ul;
#endif
}

class PopularSitesTest : public ::testing::Test {
 protected:
  PopularSitesTest()
      : kWikipedia{
            {kTitle, "Wikipedia, fhta Ph'nglui mglw'nafh"},
            {kUrl, "https://zz.m.wikipedia.org/"},
            {kLargeIconUrl, "https://zz.m.wikipedia.org/wikipedia.png"},
        },
        kYouTube{
            {kTitle, "YouTube"},
            {kUrl, "https://m.youtube.com/"},
            {kLargeIconUrl, "https://s.ytimg.com/apple-touch-icon.png"},
        },
        kChromium{
            {kTitle, "The Chromium Project"},
            {kUrl, "https://www.chromium.org/"},
            {kFaviconUrl, "https://www.chromium.org/favicon.ico"},
        },
        worker_pool_owner_(2, "PopularSitesTest."),
        prefs_(new sync_preferences::TestingPrefServiceSyncable()),
        url_fetcher_factory_(nullptr) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNTPPopularSites);
    PopularSitesImpl::RegisterProfilePrefs(prefs_->registry());
    CHECK(scoped_cache_dir_.CreateUniqueTempDir());
    cache_dir_ = scoped_cache_dir_.GetPath();
  }

  void SetCountryAndVersion(const std::string& country,
                            const std::string& version) {
    prefs_->SetString(prefs::kPopularSitesOverrideCountry, country);
    prefs_->SetString(prefs::kPopularSitesOverrideVersion, version);
  }

  void RespondWithJSON(const std::string& url,
                       const TestPopularSiteVector& sites) {
    base::ListValue sites_value;
    for (const TestPopularSite& site : sites) {
      auto site_value = base::MakeUnique<base::DictionaryValue>();
      for (const std::pair<std::string, std::string>& kv : site) {
        site_value->SetString(kv.first, kv.second);
      }
      sites_value.Append(std::move(site_value));
    }
    std::string sites_string;
    base::JSONWriter::Write(sites_value, &sites_string);
    url_fetcher_factory_.SetFakeResponse(GURL(url), sites_string, net::HTTP_OK,
                                         net::URLRequestStatus::SUCCESS);
  }

  void RespondWithData(const std::string& url, const std::string& data) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), data, net::HTTP_OK,
                                         net::URLRequestStatus::SUCCESS);
  }

  void RespondWith404(const std::string& url) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), "404", net::HTTP_NOT_FOUND,
                                         net::URLRequestStatus::SUCCESS);
  }

  void ReregisterProfilePrefs() {
    prefs_ = base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>();
    PopularSitesImpl::RegisterProfilePrefs(prefs_->registry());
  }

  // Returns an optional bool representing whether the completion callback was
  // called at all, and if yes which was the returned bool value.
  base::Optional<bool> FetchPopularSites(bool force_download,
                                         PopularSites::SitesVector* sites) {
    scoped_refptr<net::TestURLRequestContextGetter> url_request_context(
        new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get()));
    std::unique_ptr<PopularSites> popular_sites =
        CreatePopularSites(url_request_context.get());

    base::RunLoop loop;
    base::Optional<bool> save_success;
    if (popular_sites->MaybeStartFetch(
            force_download, base::Bind(
                                [](base::Optional<bool>* save_success,
                                   base::RunLoop* loop, bool success) {
                                  save_success->emplace(success);
                                  loop->Quit();
                                },
                                &save_success, &loop))) {
      loop.Run();
    }
    *sites = popular_sites->sites();
    return save_success;
  }

  std::unique_ptr<PopularSites> CreatePopularSites(
      net::URLRequestContextGetter* context) {
    return base::MakeUnique<PopularSitesImpl>(
        worker_pool_owner_.pool().get(), prefs_.get(),
        /*template_url_service=*/nullptr,
        /*variations_service=*/nullptr, context, cache_dir_,
        base::Bind(JsonUnsafeParser::Parse));
  }

  const TestPopularSite kWikipedia;
  const TestPopularSite kYouTube;
  const TestPopularSite kChromium;

  base::MessageLoopForUI ui_loop_;
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  base::ScopedTempDir scoped_cache_dir_;
  base::FilePath cache_dir_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
};

TEST_F(PopularSitesTest, ContainsDefaultTilesRightAfterConstruction) {
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context(
      new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get()));

  auto popular_sites = CreatePopularSites(url_request_context.get());
  EXPECT_THAT(popular_sites->sites().size(),
              Eq(GetNumberOfDefaultPopularSitesForPlatform()));
}

TEST_F(PopularSitesTest, IsEmptyOnConstructionIfDisabledByTrial) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndDisableFeature(kPopularSitesBakedInContentFeature);
  ReregisterProfilePrefs();

  scoped_refptr<net::TestURLRequestContextGetter> url_request_context(
      new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get()));
  auto popular_sites = CreatePopularSites(url_request_context.get());

  EXPECT_THAT(popular_sites->sites().size(), Eq(0ul));
}

TEST_F(PopularSitesTest, ShouldSucceedFetching) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));

  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].title, Str16Eq("Wikipedia, fhta Ph'nglui mglw'nafh"));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://zz.m.wikipedia.org/wikipedia.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
}

TEST_F(PopularSitesTest, Fallback) {
  SetCountryAndVersion("ZZ", "9");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kYouTube, kChromium});

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));

  ASSERT_THAT(sites.size(), Eq(2u));
  EXPECT_THAT(sites[0].title, Str16Eq("YouTube"));
  EXPECT_THAT(sites[0].url, URLEq("https://m.youtube.com/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://s.ytimg.com/apple-touch-icon.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
  EXPECT_THAT(sites[1].title, Str16Eq("The Chromium Project"));
  EXPECT_THAT(sites[1].url, URLEq("https://www.chromium.org/"));
  EXPECT_THAT(sites[1].large_icon_url, URLEq(""));
  EXPECT_THAT(sites[1].favicon_url,
              URLEq("https://www.chromium.org/favicon.ico"));
}

TEST_F(PopularSitesTest, PopulatesWithDefaultResoucesOnFailure) {
  SetCountryAndVersion("ZZ", "9");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json");

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(false)));
  EXPECT_THAT(sites.size(), Eq(GetNumberOfDefaultPopularSitesForPlatform()));
}

TEST_F(PopularSitesTest, AddsIconResourcesToDefaultPages) {
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context(
      new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get()));
  std::unique_ptr<PopularSites> popular_sites =
      CreatePopularSites(url_request_context.get());

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_ANDROID) || defined(OS_IOS))
  ASSERT_FALSE(popular_sites->sites().empty());
  for (const auto& site : popular_sites->sites()) {
    EXPECT_THAT(site.default_icon_resource, Gt(0));
  }
#endif
}

TEST_F(PopularSitesTest, ProvidesDefaultSitesUntilCallbackReturns) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context(
      new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get()));
  std::unique_ptr<PopularSites> popular_sites =
      CreatePopularSites(url_request_context.get());

  base::RunLoop loop;
  base::Optional<bool> save_success = false;

  bool callback_was_scheduled = popular_sites->MaybeStartFetch(
      /*force_download=*/true, base::Bind(
                                   [](base::Optional<bool>* save_success,
                                      base::RunLoop* loop, bool success) {
                                     save_success->emplace(success);
                                     loop->Quit();
                                   },
                                   &save_success, &loop));

  // Assert that callback was scheduled so we can wait for its completion.
  ASSERT_TRUE(callback_was_scheduled);
  // There should be 8 default sites as nothing was fetched yet.
  EXPECT_THAT(popular_sites->sites().size(),
              Eq(GetNumberOfDefaultPopularSitesForPlatform()));

  loop.Run();  // Wait for the fetch to finish and the callback to return.

  EXPECT_TRUE(save_success.value());
  // The 1 fetched site should replace the default sites.
  EXPECT_THAT(popular_sites->sites().size(), Eq(1ul));
}

TEST_F(PopularSitesTest, ClearsCacheFileFromOldVersions) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  PopularSites::SitesVector sites;
  const base::FilePath old_cache_path =
      cache_dir_.AppendASCII("suggested_sites.json");
  CHECK(base::ImportantFileWriter::WriteFileAtomically(old_cache_path,
                                                       "Old cache"));
  FetchPopularSites(/*force_download=*/false, &sites);
  worker_pool_owner_.pool()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(old_cache_path));
}

TEST_F(PopularSitesTest, UsesCachedJson) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  PopularSites::SitesVector sites;
  ASSERT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));

  // File disappears from server, but we don't need it because it's cached.
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::nullopt));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
}

TEST_F(PopularSitesTest, CachesEmptyFile) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithData(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json", "[]");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kWikipedia});

  // First request succeeds and caches empty suggestions list (no fallback).
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));
  EXPECT_THAT(sites, IsEmpty());

  // File appears on server, but we continue to use our cached empty file.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::nullopt));
  EXPECT_THAT(sites, IsEmpty());
}

TEST_F(PopularSitesTest, DoesntUseCachedFileIfDownloadForced) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/true, &sites),
              Eq(base::Optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // File disappears from server. Download is forced, so we get the new file.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kChromium});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/true, &sites),
              Eq(base::Optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, RefetchesAfterCountryMoved) {
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZX_9.json",
      {kChromium});

  PopularSites::SitesVector sites;

  // First request (in ZZ) saves Wikipedia.
  SetCountryAndVersion("ZZ", "9");
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // Second request (now in ZX) saves Chromium.
  SetCountryAndVersion("ZX", "9");
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              base::Optional<bool>(true));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, DoesntCacheInvalidFile) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithData(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      "ceci n'est pas un json");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json");

  // First request falls back and gets nothing there either.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(false)));

  // Second request refetches ZZ_9, which now has data.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kChromium});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, RefetchesAfterFallback) {
  SetCountryAndVersion("ZZ", "9");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kWikipedia});

  // First request falls back.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // Second request refetches ZZ_9, which now has data.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kChromium});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(base::Optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

}  // namespace
}  // namespace ntp_tiles
