// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/whitelist_checker_client.h"

#include "base/bind.h"

namespace safe_browsing {

// Static
void WhitelistCheckerClient::StartCheckCsdWhitelist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::Callback<void(bool)> callback_for_result) {
  // TODO(nparker): Maybe also call SafeBrowsingDatabaseManager::CanCheckUrl()
  if (!url.is_valid()) {
    callback_for_result.Run(true /* is_whitelisted */);
    return;
  }

  // Make a client for each request. The caller could have several in
  // flight at once.
  std::unique_ptr<WhitelistCheckerClient> client =
      base::MakeUnique<WhitelistCheckerClient>(callback_for_result);
  AsyncMatch match = database_manager->CheckCsdWhitelistUrl(url, client.get());

  switch (match) {
    case AsyncMatch::MATCH:
      callback_for_result.Run(true /* is_whitelisted */);
      break;
    case AsyncMatch::NO_MATCH:
      callback_for_result.Run(false /* is_whitelisted */);
      break;
    case AsyncMatch::ASYNC:
      // Client is now self-owned. When it gets called back with the result,
      // it will delete itself.
      client.release();
      break;
  }
}

WhitelistCheckerClient::WhitelistCheckerClient(
    base::Callback<void(bool)> callback_for_result)
    : callback_for_result_(callback_for_result), weak_factory_(this) {
  // Set a timer to fail open, i.e. call it "whitelisted", if the full
  // check takes too long.
  auto timeout_callback =
      base::Bind(&WhitelistCheckerClient::OnCheckWhitelistUrlResult,
                 weak_factory_.GetWeakPtr(), true /* is_whitelisted */);
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMsec),
               timeout_callback);
}

WhitelistCheckerClient::~WhitelistCheckerClient() {}

// SafeBrowsingDatabaseMananger::Client impl
void WhitelistCheckerClient::OnCheckWhitelistUrlResult(bool is_whitelisted) {
  // This method is invoked only if we're already self-owned.
  callback_for_result_.Run(is_whitelisted);
  delete this;
}

}  // namespace safe_browsing
