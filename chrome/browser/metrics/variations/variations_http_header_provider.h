// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_HTTP_HEADER_PROVIDER_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_HTTP_HEADER_PROVIDER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/metrics/field_trial.h"
#include "base/synchronization/lock.h"
#include "chrome/common/metrics/variations/variation_ids.h"

namespace content {
class ResourceContext;
}

namespace net {
class HttpRequestHeaders;
}

class GURL;
class Profile;
class ProfileIOData;

template <typename T> struct DefaultSingletonTraits;

namespace chrome_variations {

// A helper class for maintaining Chrome experiments and metrics state
// transmitted in custom HTTP request headers.
// This class is a thread-safe singleton.
class VariationsHttpHeaderProvider : base::FieldTrialList::Observer {
 public:
  static VariationsHttpHeaderProvider* GetInstance();

  // Adds Chrome experiment and metrics state as custom headers to |headers|.
  // Some headers may not be set given the |incognito| mode or whether
  // the user has |uma_enabled|.  Also, we never transmit headers to non-Google
  // sites, which is checked based on the destination |url|.
  void AppendHeaders(const GURL& url,
                     bool incognito,
                     bool uma_enabled,
                     net::HttpRequestHeaders* headers);

 private:
  friend struct DefaultSingletonTraits<VariationsHttpHeaderProvider>;

  VariationsHttpHeaderProvider();
  virtual ~VariationsHttpHeaderProvider();

  // base::FieldTrialList::Observer implementation.
  // This will add the variation ID associated with |trial_name| and
  // |group_name| to the variation ID cache.
  virtual void OnFieldTrialGroupFinalized(
      const std::string& trial_name,
      const std::string& group_name) OVERRIDE;

  // Prepares the variation IDs cache with initial values if not already done.
  // This method also registers the caller with the FieldTrialList to receive
  // new variation IDs.
  void InitVariationIDsCacheIfNeeded();

  // Takes whatever is currently in |variation_ids_set_| and recreates
  // |variation_ids_header_| with it.  Assumes the the |lock_| is currently
  // held.
  void UpdateVariationIDsHeaderValue();

  // Guards |variation_ids_cache_initialized_|, |variation_ids_set_| and
  // |variation_ids_header_|.
  base::Lock lock_;

  // Whether or not we've initialized the cache.
  bool variation_ids_cache_initialized_;

  // Keep a cache of variation IDs that are transmitted in headers to Google.
  // This consists of a list of valid IDs, and the actual transmitted header.
  std::set<chrome_variations::VariationID> variation_ids_set_;
  std::string variation_ids_header_;

  DISALLOW_COPY_AND_ASSIGN(VariationsHttpHeaderProvider);
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_HTTP_HEADER_PROVIDER_H_
