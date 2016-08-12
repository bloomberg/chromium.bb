// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blacklist_state_fetcher.h"

#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/url_request/url_request_test_util.h"

namespace extensions {
namespace {

static const char kUrlPrefix[] = "https://prefix.com/foo";
static const char kBackupConnectUrlPrefix[] = "https://alt1-prefix.com/foo";
static const char kBackupHttpUrlPrefix[] = "https://alt2-prefix.com/foo";
static const char kBackupNetworkUrlPrefix[] = "https://alt3-prefix.com/foo";
static const char kClient[] = "unittest";
static const char kAppVer[] = "1.0";

safe_browsing::SafeBrowsingProtocolConfig CreateSafeBrowsingProtocolConfig() {
  safe_browsing::SafeBrowsingProtocolConfig config;
  config.client_name = kClient;
  config.url_prefix = kUrlPrefix;
  config.backup_connect_error_url_prefix = kBackupConnectUrlPrefix;
  config.backup_http_error_url_prefix = kBackupHttpUrlPrefix;
  config.backup_network_error_url_prefix = kBackupNetworkUrlPrefix;
  config.version = kAppVer;
  return config;
}

}  // namespace

TestBlacklistStateFetcher::TestBlacklistStateFetcher(
    BlacklistStateFetcher* fetcher) : fetcher_(fetcher) {
  fetcher_->SetSafeBrowsingConfig(CreateSafeBrowsingProtocolConfig());
  scoped_refptr<net::TestURLRequestContextGetter> context =
        new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get());
  fetcher_->SetURLRequestContextForTest(context.get());
}

TestBlacklistStateFetcher::~TestBlacklistStateFetcher() {
}

void TestBlacklistStateFetcher::SetBlacklistVerdict(
    const std::string& id, ClientCRXListInfoResponse_Verdict state) {
  verdicts_[id] = state;
}

bool TestBlacklistStateFetcher::HandleFetcher(int fetcher_id) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(
      fetcher_id);
  if (!url_fetcher)
    return false;

  const std::string& request_str = url_fetcher->upload_data();
  ClientCRXListInfoRequest request;
  if (!request.ParseFromString(request_str))
    return false;

  std::string id = request.id();

  ClientCRXListInfoResponse response;
  if (base::ContainsKey(verdicts_, id))
    response.set_verdict(verdicts_[id]);
  else
    response.set_verdict(ClientCRXListInfoResponse::NOT_IN_BLACKLIST);

  std::string response_str;
  response.SerializeToString(&response_str);

  url_fetcher->set_status(net::URLRequestStatus());
  url_fetcher->set_response_code(200);
  url_fetcher->SetResponseString(response_str);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);

  return true;
}

}  // namespace extensions
