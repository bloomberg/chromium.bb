// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_SDCH_POLICY_H_
#define CHROME_BROWSER_NET_CHROME_SDCH_POLICY_H_

#include <string>

#include "net/base/sdch_observer.h"
#include "net/url_request/sdch_dictionary_fetcher.h"

class GURL;

namespace net {
class SdchManager;
class URLRequestContext;
}

// Implementation of chrome embedder policy for SDCH.  Handles fetches.
// TODO(rdsmith): Implement dictionary prioritization.
class ChromeSdchPolicy : public net::SdchObserver {
 public:
  // Consumer must guarantee that |sdch_manager| and |context| outlive
  // this object.
  ChromeSdchPolicy(net::SdchManager* sdch_manager,
                   net::URLRequestContext* context);
  ~ChromeSdchPolicy() override;

  void OnDictionaryFetched(const std::string& dictionary_text,
                           const GURL& dictionary_url,
                           const net::BoundNetLog& net_log);

  // SdchObserver implementation.
  void OnGetDictionary(net::SdchManager* manager,
                       const GURL& request_url,
                       const GURL& dictionary_url) override;
  void OnClearDictionaries(net::SdchManager* manager) override;

 private:
  net::SdchManager* manager_;
  net::SdchDictionaryFetcher fetcher_;
};

#endif  // CHROME_BROWSER_NET_CHROME_SDCH_POLICY_H_
