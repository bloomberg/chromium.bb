// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_sdch_policy.h"

#include "base/bind.h"
#include "net/base/sdch_manager.h"
#include "net/base/sdch_net_log_params.h"

ChromeSdchPolicy::ChromeSdchPolicy(net::SdchManager* sdch_manager,
                                   net::URLRequestContext* context)
    : manager_(sdch_manager),
      // Because |fetcher_| is owned by ChromeSdchPolicy, the
      // ChromeSdchPolicy object will be available for the lifetime
      // of |fetcher_|.
      fetcher_(context,
               base::Bind(&ChromeSdchPolicy::OnDictionaryFetched,
                          base::Unretained(this))) {
  manager_->AddObserver(this);
}

ChromeSdchPolicy::~ChromeSdchPolicy() {
  manager_->RemoveObserver(this);
}

void ChromeSdchPolicy::OnDictionaryFetched(const std::string& dictionary_text,
                                           const GURL& dictionary_url,
                                           const net::BoundNetLog& net_log) {
  net::SdchProblemCode rv =
      manager_->AddSdchDictionary(dictionary_text, dictionary_url);
  if (rv != net::SDCH_OK) {
    net::SdchManager::SdchErrorRecovery(rv);
    net_log.AddEvent(net::NetLog::TYPE_SDCH_DICTIONARY_ERROR,
                     base::Bind(&net::NetLogSdchDictionaryFetchProblemCallback,
                                rv, dictionary_url, true));
  }
}

void ChromeSdchPolicy::OnGetDictionary(net::SdchManager* manager,
                                       const GURL& request_url,
                                       const GURL& dictionary_url) {
  fetcher_.Schedule(dictionary_url);
}

void ChromeSdchPolicy::OnClearDictionaries(net::SdchManager* manager) {
  fetcher_.Cancel();
}
