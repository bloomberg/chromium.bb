// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mod_pagespeed/mod_pagespeed_metrics.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mod_pagespeed {

// Ensure that we count PageSpeed headers correctly.
TEST(ModPagespeedMetricsTest, CountPageSpeedHeadersTest) {
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
  int num_bucket_1 = 0;   // unrecognized format/value bucket
  int num_bucket_30 = 0;  // 1.2.24.1 bucket
  int num_bucket_33 = 0;  // 1.3.25.2 bucket

  std::unique_ptr<base::HistogramSamples> server_samples;
  std::unique_ptr<base::HistogramSamples> version_samples;

  // No PageSpeed header. The VersionCounts histogram isn't created yet.
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  base::HistogramBase* server_histogram =
      base::StatisticsRecorder::FindHistogram(
          "Prerender.PagespeedHeader.ServerCounts");
  ASSERT_TRUE(server_histogram != NULL);
  ASSERT_TRUE(NULL == base::StatisticsRecorder::FindHistogram(
                          "Prerender.PagespeedHeader.VersionCounts"));

  server_samples = server_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(num_mps, server_samples->GetCount(1));
  EXPECT_EQ(num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(num_other, server_samples->GetCount(4));

  // X-Mod-Pagespeed header in expected format. VersionCounts now exists.
  headers->AddHeader("X-Mod-Pagespeed: 1.2.24.1-2300");
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  base::HistogramBase* version_histogram =
      base::StatisticsRecorder::FindHistogram(
          "Prerender.PagespeedHeader.VersionCounts");
  ASSERT_TRUE(version_histogram != NULL);
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(++num_mps, server_samples->GetCount(1));
  EXPECT_EQ(num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(num_other, server_samples->GetCount(4));
  EXPECT_EQ(num_bucket_1, version_samples->GetCount(1));
  EXPECT_EQ(++num_bucket_30, version_samples->GetCount(30));  // +1 for #30
  EXPECT_EQ(num_bucket_33, version_samples->GetCount(33));
  headers->RemoveHeader("X-Mod-Pagespeed");

  // X-Mod-Pagespeed header in unexpected format.
  headers->AddHeader("X-Mod-Pagespeed: Powered By PageSpeed!");
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(++num_mps, server_samples->GetCount(1));
  EXPECT_EQ(num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(num_other, server_samples->GetCount(4));
  EXPECT_EQ(++num_bucket_1, version_samples->GetCount(1));  // +1 for 'huh?'
  EXPECT_EQ(num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(num_bucket_33, version_samples->GetCount(33));
  headers->RemoveHeader("X-Mod-Pagespeed");

  // X-Page-Speed header in mod_pagespeed format (so ngx_pagespeed).
  headers->AddHeader("X-Page-Speed: 1.3.25.2-2530");
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(num_mps, server_samples->GetCount(1));
  EXPECT_EQ(++num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(num_other, server_samples->GetCount(4));
  EXPECT_EQ(num_bucket_1, version_samples->GetCount(1));
  EXPECT_EQ(num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(++num_bucket_33, version_samples->GetCount(33));  // +1 for #33
  headers->RemoveHeader("X-Page-Speed");

  // X-Page-Speed header in PageSpeed Service format.
  headers->AddHeader("X-Page-Speed: 97_4_bo");
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(num_mps, server_samples->GetCount(1));  // no change
  EXPECT_EQ(num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(++num_pss, server_samples->GetCount(3));  // +1 for PSS
  EXPECT_EQ(num_other, server_samples->GetCount(4));
  EXPECT_EQ(num_bucket_1, version_samples->GetCount(1));
  EXPECT_EQ(num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(num_bucket_33, version_samples->GetCount(33));
  headers->RemoveHeader("X-Page-Speed");

  // X-Page-Speed header in an unrecognized format (IISpeed in this case).
  headers->AddHeader("X-Page-Speed: 1.0PS1.2-20130615");
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(++num_responses, server_samples->GetCount(0));
  EXPECT_EQ(num_mps, server_samples->GetCount(1));  // no change
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(++num_other, server_samples->GetCount(4));  // +1 for 'other'
  EXPECT_EQ(num_bucket_1, version_samples->GetCount(1));
  EXPECT_EQ(num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(num_bucket_33, version_samples->GetCount(33));

  // Not a main frame => not counted at all.
  RecordMetrics(content::RESOURCE_TYPE_SUB_FRAME, url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(num_responses, server_samples->GetCount(0));
  EXPECT_EQ(num_mps, server_samples->GetCount(1));
  EXPECT_EQ(num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(num_other, server_samples->GetCount(4));
  EXPECT_EQ(num_bucket_1, version_samples->GetCount(1));
  EXPECT_EQ(num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(num_bucket_33, version_samples->GetCount(33));

  // Not a http/https URL => not counted at all.
  GURL data_url("data:image/png;base64,yadda yadda==");
  RecordMetrics(content::RESOURCE_TYPE_MAIN_FRAME, data_url, headers.get());
  server_samples = server_histogram->SnapshotSamples();
  version_samples = version_histogram->SnapshotSamples();
  EXPECT_EQ(num_responses, server_samples->GetCount(0));
  EXPECT_EQ(num_mps, server_samples->GetCount(1));
  EXPECT_EQ(num_ngx, server_samples->GetCount(2));
  EXPECT_EQ(num_pss, server_samples->GetCount(3));
  EXPECT_EQ(num_other, server_samples->GetCount(4));
  EXPECT_EQ(num_bucket_1, version_samples->GetCount(1));
  EXPECT_EQ(num_bucket_30, version_samples->GetCount(30));
  EXPECT_EQ(num_bucket_33, version_samples->GetCount(33));

  headers->RemoveHeader("X-Page-Speed");
}

}  // namespace mod_pagespeed
