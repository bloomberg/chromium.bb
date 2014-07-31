// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::ResourceType;

namespace prerender {

class PrerenderUtilTest : public testing::Test {
 public:
  PrerenderUtilTest() {
  }
};

// Ensure that extracting a urlencoded URL in the url= query string component
// works.
TEST_F(PrerenderUtilTest, ExtractURLInQueryStringTest) {
  GURL result;
  EXPECT_TRUE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/url?sa=t&source=web&cd=1&ved=0CBcQFjAA&url=http%3A%2F%2Fwww.abercrombie.com%2Fwebapp%2Fwcs%2Fstores%2Fservlet%2FStoreLocator%3FcatalogId%3D%26storeId%3D10051%26langId%3D-1&rct=j&q=allinurl%3A%26&ei=KLyUTYGSEdTWiAKUmLCdCQ&usg=AFQjCNF8nJ2MpBFfr1ijO39_f22bcKyccw&sig2=2ymyGpO0unJwU1d4kdCUjQ"),
      &result));
  ASSERT_EQ(GURL("http://www.abercrombie.com/webapp/wcs/stores/servlet/StoreLocator?catalogId=&storeId=10051&langId=-1").spec(), result.spec());
  EXPECT_FALSE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/url?sadf=test&blah=blahblahblah"), &result));
  EXPECT_FALSE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/?url=INVALIDurlsAREsoMUCHfun.com"), &result));
  EXPECT_TRUE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/?url=http://validURLSareGREAT.com"),
      &result));
  ASSERT_EQ(GURL("http://validURLSareGREAT.com").spec(), result.spec());
}

// Ensure that extracting an experiment in the lpe= query string component
// works.
TEST_F(PrerenderUtilTest, ExtractExperimentInQueryStringTest) {
  EXPECT_EQ(GetQueryStringBasedExperiment(
      GURL("http://www.google.com/url?sa=t&source=web&cd=1&ved=0CBcQFjAA&url=http%3A%2F%2Fwww.abercrombie.com%2Fwebapp%2Fwcs%2Fstores%2Fservlet%2FStoreLocator%3FcatalogId%3D%26storeId%3D10051%26langId%3D-1&rct=j&q=allinurl%3A%26&ei=KLyUTYGSEdTWiAKUmLCdCQ&usg=AFQjCNF8nJ2MpBFfr1ijO39_f22bcKyccw&sig2=2ymyGpO0unJwU1d4kdCUjQ&lpe=4&asdf=test")), 4);
  EXPECT_EQ(GetQueryStringBasedExperiment(
      GURL("http://www.google.com/test.php?a=b")), kNoExperiment);
  EXPECT_EQ(GetQueryStringBasedExperiment(
      GURL("http://www.google.com/test.php?lpe=5")), 5);
  EXPECT_EQ(GetQueryStringBasedExperiment(
      GURL("http://www.google.com/test.php?lpe=50")), kNoExperiment);
  EXPECT_EQ(GetQueryStringBasedExperiment(
      GURL("http://www.google.com/test.php?lpe=0")), kNoExperiment);
  EXPECT_EQ(GetQueryStringBasedExperiment(
      GURL("http://www.google.com/test.php?lpe=10")), kNoExperiment);
}

// Ensure that we detect Google search result URLs correctly.
TEST_F(PrerenderUtilTest, DetectGoogleSearchREsultURLTest) {
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/#asdf")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/?a=b")));
  EXPECT_TRUE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/search?q=hi")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/search")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/webhp")));
  EXPECT_TRUE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/webhp?a=b#123")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://www.google.com/imgres")));
  EXPECT_FALSE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/imgres?q=hi")));
  EXPECT_FALSE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/imgres?q=hi#123")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://google.com/search")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://WWW.GooGLE.CoM/search")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://WWW.GooGLE.CoM/SeArcH")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.co.uk/search")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://google.co.uk/search")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://www.chromium.org/search")));
}

// Ensure that we count PageSpeed headers correctly.
TEST_F(PrerenderUtilTest, CountPageSpeedHeadersTest) {
  base::StatisticsRecorder::Initialize();
  GURL url("http://google.com");
  std::string temp("HTTP/1.1 200 OK\n\n");
  std::replace(temp.begin(), temp.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(temp));

  int num_responses = 0;
  int num_mps = 0;
  int num_ngx = 0;
  int num_pss = 0;
  int num_other = 0;
  int num_bucket_1  = 0;  // unrecognized format/value bucket
  int num_bucket_30 = 0;  // 1.2.24.1 bucket
  int num_bucket_33 = 0;  // 1.3.25.2 bucket

  scoped_ptr<base::HistogramSamples> server_samples;
  scoped_ptr<base::HistogramSamples> version_samples;

  // No PageSpeed header. The VersionCounts histogram isn't created yet.
  GatherPagespeedData(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  base::HistogramBase* server_histogram =
      base::StatisticsRecorder::FindHistogram(
          "Prerender.PagespeedHeader.ServerCounts");
  ASSERT_TRUE(server_histogram != NULL);
  ASSERT_TRUE(NULL == base::StatisticsRecorder::FindHistogram(
      "Prerender.PagespeedHeader.VersionCounts"));

  server_samples = server_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(  num_mps,       server_samples->GetCount(1));
  EXPECT_EQ(  num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));

  // X-Mod-Pagespeed header in expected format. VersionCounts now exists.
  headers->AddHeader("X-Mod-Pagespeed: 1.2.24.1-2300");
  GatherPagespeedData(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  base::HistogramBase* version_histogram =
      base::StatisticsRecorder::FindHistogram(
          "Prerender.PagespeedHeader.VersionCounts");
  ASSERT_TRUE(version_histogram != NULL);
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(++num_mps,       server_samples->GetCount(1));
  EXPECT_EQ(  num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));
  EXPECT_EQ(  num_bucket_1,  version_samples->GetCount(1));
  EXPECT_EQ(++num_bucket_30, version_samples->GetCount(30));  // +1 for #30
  EXPECT_EQ(  num_bucket_33, version_samples->GetCount(33));
  headers->RemoveHeader("X-Mod-Pagespeed");

  // X-Mod-Pagespeed header in unexpected format.
  headers->AddHeader("X-Mod-Pagespeed: Powered By PageSpeed!");
  GatherPagespeedData(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(++num_mps,       server_samples->GetCount(1));
  EXPECT_EQ(  num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));
  EXPECT_EQ(++num_bucket_1,  version_samples->GetCount(1));   // +1 for 'huh?'
  EXPECT_EQ(  num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(  num_bucket_33, version_samples->GetCount(33));
  headers->RemoveHeader("X-Mod-Pagespeed");

  // X-Page-Speed header in mod_pagespeed format (so ngx_pagespeed).
  headers->AddHeader("X-Page-Speed: 1.3.25.2-2530");
  GatherPagespeedData(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(  num_mps,       server_samples->GetCount(1));
  EXPECT_EQ(++num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));
  EXPECT_EQ(  num_bucket_1,  version_samples->GetCount(1));
  EXPECT_EQ(  num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(++num_bucket_33, version_samples->GetCount(33));  // +1 for #33
  headers->RemoveHeader("X-Page-Speed");

  // X-Page-Speed header in PageSpeed Service format.
  headers->AddHeader("X-Page-Speed: 97_4_bo");
  GatherPagespeedData(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(  num_mps,       server_samples->GetCount(1));    // no change
  EXPECT_EQ(  num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(++num_pss,       server_samples->GetCount(3));    // +1 for PSS
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));
  EXPECT_EQ(  num_bucket_1,  version_samples->GetCount(1));
  EXPECT_EQ(  num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(  num_bucket_33, version_samples->GetCount(33));
  headers->RemoveHeader("X-Page-Speed");

  // X-Page-Speed header in an unrecognized format (IISpeed in this case).
  headers->AddHeader("X-Page-Speed: 1.0PS1.2-20130615");
  GatherPagespeedData(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(  num_mps,       server_samples->GetCount(1));    // no change
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(++num_other,     server_samples->GetCount(4));    // +1 for 'other'
  EXPECT_EQ(  num_bucket_1,  version_samples->GetCount(1));
  EXPECT_EQ(  num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(  num_bucket_33, version_samples->GetCount(33));

  // Not a main frame => not counted at all.
  GatherPagespeedData(content::RESOURCE_TYPE_SUB_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(  num_responses, server_samples->GetCount(0));
  EXPECT_EQ(  num_mps,       server_samples->GetCount(1));
  EXPECT_EQ(  num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));
  EXPECT_EQ(  num_bucket_1,  version_samples->GetCount(1));
  EXPECT_EQ(  num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(  num_bucket_33, version_samples->GetCount(33));

  // Not a http/https URL => not counted at all.
  GURL data_url("data:image/png;base64,yadda yadda==");
  GatherPagespeedData(
      content::RESOURCE_TYPE_MAIN_FRAME, data_url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(  num_responses, server_samples->GetCount(0));
  EXPECT_EQ(  num_mps,       server_samples->GetCount(1));
  EXPECT_EQ(  num_ngx,       server_samples->GetCount(2));
  EXPECT_EQ(  num_pss,       server_samples->GetCount(3));
  EXPECT_EQ(  num_other,     server_samples->GetCount(4));
  EXPECT_EQ(  num_bucket_1,  version_samples->GetCount(1));
  EXPECT_EQ(  num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(  num_bucket_33, version_samples->GetCount(33));

  headers->RemoveHeader("X-Page-Speed");
}

}  // namespace prerender
