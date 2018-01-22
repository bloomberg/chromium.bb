// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"

#include "net/base/net_errors.h"

namespace net {

std::string DhcpProxyScriptFetcher::GetFetcherName() const {
  return std::string();
}

DhcpProxyScriptFetcher::DhcpProxyScriptFetcher() = default;

DhcpProxyScriptFetcher::~DhcpProxyScriptFetcher() = default;

DoNothingDhcpProxyScriptFetcher::DoNothingDhcpProxyScriptFetcher() = default;

DoNothingDhcpProxyScriptFetcher::~DoNothingDhcpProxyScriptFetcher() = default;

int DoNothingDhcpProxyScriptFetcher::Fetch(
    base::string16* utf16_text, const CompletionCallback& callback) {
  return ERR_NOT_IMPLEMENTED;
}

void DoNothingDhcpProxyScriptFetcher::Cancel() {}

void DoNothingDhcpProxyScriptFetcher::OnShutdown() {}

const GURL& DoNothingDhcpProxyScriptFetcher::GetPacURL() const {
  return gurl_;
}

std::string DoNothingDhcpProxyScriptFetcher::GetFetcherName() const {
  return "do nothing";
}

}  // namespace net
