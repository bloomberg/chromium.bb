// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_FETCHER_H_
#define CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_FETCHER_H_

#include "base/callback_forward.h"
#include "base/optional.h"

struct OneGoogleBarData;

// Interface for fetching OneGoogleBarData over the network.
class OneGoogleBarFetcher {
 public:
  using OneGoogleCallback =
      base::OnceCallback<void(const base::Optional<OneGoogleBarData>&)>;

  // Initiates a fetch from the network. On completion (successful or not), the
  // callback will be called with the result, which will be nullopt on failure.
  virtual void Fetch(OneGoogleCallback callback) = 0;
};

#endif  // CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_FETCHER_H_
