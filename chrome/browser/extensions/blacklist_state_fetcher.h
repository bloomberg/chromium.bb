// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing_db/util.h"
#include "extensions/browser/blacklist_state.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}

namespace extensions {

class BlacklistStateFetcher : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(BlacklistState)> RequestCallback;

  BlacklistStateFetcher();

  ~BlacklistStateFetcher() override;

  virtual void Request(const std::string& id, const RequestCallback& callback);

  void SetSafeBrowsingConfig(
      const safe_browsing::SafeBrowsingProtocolConfig& config);

  void SetURLRequestContextForTest(
      net::URLRequestContextGetter* request_context);

 protected:
  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  typedef std::multimap<std::string, RequestCallback> CallbackMultiMap;

  GURL RequestUrl() const;

  void SendRequest(const std::string& id);

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  std::unique_ptr<safe_browsing::SafeBrowsingProtocolConfig>
      safe_browsing_config_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<net::URLRequestContextGetter> parent_request_context_for_test_;

  // URLFetcher -> (owned fetcher, extension id).
  std::map<const net::URLFetcher*,
           std::pair<std::unique_ptr<net::URLFetcher>, std::string>>
      requests_;

  // Callbacks by extension ID.
  CallbackMultiMap callbacks_;

  base::WeakPtrFactory<BlacklistStateFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistStateFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_
