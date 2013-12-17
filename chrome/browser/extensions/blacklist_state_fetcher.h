// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/blacklist_state.h"
#include "chrome/browser/safe_browsing/protocol_manager_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace extensions {

class BlacklistStateFetcher : public net::URLFetcherDelegate {
 public:
  BlacklistStateFetcher();

  virtual ~BlacklistStateFetcher();

  typedef base::Callback<void(BlacklistState)> RequestCallback;

  virtual void Request(const std::string& id, const RequestCallback& callback);

  void SetSafeBrowsingConfig(const SafeBrowsingProtocolConfig& config);

 protected:
  // net::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  GURL RequestUrl() const;

  SafeBrowsingProtocolConfig safe_browsing_config_;
  bool safe_browsing_config_initialized_;

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  // Extension id by URLFetcher.
  std::map<const net::URLFetcher*, std::string> requests_;

  typedef std::multimap<std::string, RequestCallback> CallbackMultiMap;

  // Callbacks by extension ID.
  CallbackMultiMap callbacks_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistStateFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_

