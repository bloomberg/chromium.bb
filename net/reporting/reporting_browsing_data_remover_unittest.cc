// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_browsing_data_remover.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_client.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class ReportingBrowsingDataRemoverTest : public ReportingTestBase {
 protected:
  void RemoveBrowsingData(bool remove_reports,
                          bool remove_clients,
                          std::string host) {
    int data_type_mask = 0;
    if (remove_reports)
      data_type_mask |= ReportingBrowsingDataRemover::DATA_TYPE_REPORTS;
    if (remove_clients)
      data_type_mask |= ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS;

    base::Callback<bool(const GURL&)> origin_filter;
    if (!host.empty()) {
      origin_filter =
          base::Bind(&ReportingBrowsingDataRemoverTest::HostIs, host);
    }

    browsing_data_remover()->RemoveBrowsingData(data_type_mask, origin_filter);
  }

  static bool HostIs(std::string host, const GURL& url) {
    return url.host() == host;
  }

  size_t report_count() {
    std::vector<const ReportingReport*> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  size_t client_count() {
    std::vector<const ReportingClient*> clients;
    cache()->GetClients(&clients);
    return clients.size();
  }

  const GURL kUrl1_ = GURL("https://origin1/path");
  const GURL kUrl2_ = GURL("https://origin2/path");
  const url::Origin kOrigin1_ = url::Origin(kUrl1_);
  const url::Origin kOrigin2_ = url::Origin(kUrl2_);
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kGroup_ = "group";
  const std::string kType_ = "default";
};

TEST_F(ReportingBrowsingDataRemoverTest, RemoveNothing) {
  cache()->AddReport(kUrl1_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl2_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->SetClient(kOrigin1_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));
  cache()->SetClient(kOrigin2_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));

  RemoveBrowsingData(/* remove_reports= */ false, /* remove_clients= */ false,
                     /* host= */ "");
  EXPECT_EQ(2u, report_count());
  EXPECT_EQ(2u, client_count());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveAllReports) {
  cache()->AddReport(kUrl1_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl2_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->SetClient(kOrigin1_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));
  cache()->SetClient(kOrigin2_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));

  RemoveBrowsingData(/* remove_reports= */ true, /* remove_clients= */ false,
                     /* host= */ "");
  EXPECT_EQ(0u, report_count());
  EXPECT_EQ(2u, client_count());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveAllClients) {
  cache()->AddReport(kUrl1_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl2_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->SetClient(kOrigin1_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));
  cache()->SetClient(kOrigin2_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));

  RemoveBrowsingData(/* remove_reports= */ false, /* remove_clients= */ true,
                     /* host= */ "");
  EXPECT_EQ(2u, report_count());
  EXPECT_EQ(0u, client_count());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveAllReportsAndClients) {
  cache()->AddReport(kUrl1_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl2_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->SetClient(kOrigin1_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));
  cache()->SetClient(kOrigin2_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));

  RemoveBrowsingData(/* remove_reports= */ true, /* remove_clients= */ true,
                     /* host= */ "");
  EXPECT_EQ(0u, report_count());
  EXPECT_EQ(0u, client_count());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveSomeReports) {
  cache()->AddReport(kUrl1_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl2_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->SetClient(kOrigin1_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));
  cache()->SetClient(kOrigin2_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));

  RemoveBrowsingData(/* remove_reports= */ true, /* remove_clients= */ false,
                     /* host= */ kUrl1_.host());
  EXPECT_EQ(2u, client_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl2_, reports[0]->url);
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveSomeClients) {
  cache()->AddReport(kUrl1_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl2_, kGroup_, kType_,
                     base::MakeUnique<base::DictionaryValue>(),
                     tick_clock()->NowTicks(), 0);
  cache()->SetClient(kOrigin1_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));
  cache()->SetClient(kOrigin2_, kEndpoint_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup_,
                     tick_clock()->NowTicks() + base::TimeDelta::FromDays(7));

  RemoveBrowsingData(/* remove_reports= */ false, /* remove_clients= */ true,
                     /* host= */ kUrl1_.host());
  EXPECT_EQ(2u, report_count());
  EXPECT_FALSE(FindClientInCache(cache(), kOrigin1_, kEndpoint_) != nullptr);
  EXPECT_TRUE(FindClientInCache(cache(), kOrigin2_, kEndpoint_) != nullptr);
}

}  // namespace
}  // namespace net
