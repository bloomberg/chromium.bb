// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_http_header_provider.h"

#include "base/base64.h"
#include "base/memory/singleton.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/metrics/proto/chrome_experiments.pb.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"

using content::BrowserThread;

namespace chrome_variations {

VariationsHttpHeaderProvider* VariationsHttpHeaderProvider::GetInstance() {
  return Singleton<VariationsHttpHeaderProvider>::get();
}

void VariationsHttpHeaderProvider::AppendHeaders(
    const GURL& url,
    bool incognito,
    bool uma_enabled,
    net::HttpRequestHeaders* headers) {
  // Note the criteria for attaching Chrome experiment headers:
  // 1. We only transmit to *.google.<TLD> domains. NOTE that this use of
  //    google_util helpers to check this does not guarantee that the URL is
  //    Google-owned, only that it is of the form *.google.<TLD>. In the future
  //    we may choose to reinforce this check.
  // 2. Only transmit for non-Incognito profiles.
  // 3. For the X-Chrome-UMA-Enabled bit, only set it if UMA is in fact enabled
  //    for this install of Chrome.
  // 4. For the X-Chrome-Variations, only include non-empty variation IDs.
  if (incognito ||
      !google_util::IsGoogleDomainUrl(url.spec(),
                                      google_util::ALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    return;
  }

  if (uma_enabled)
    headers->SetHeaderIfMissing("X-Chrome-UMA-Enabled", "1");

  // Lazily initialize the header, if not already done, before attempting to
  // transmit it.
  InitVariationIDsCacheIfNeeded();

  std::string variation_ids_header_copy;
  {
    base::AutoLock scoped_lock(lock_);
    variation_ids_header_copy = variation_ids_header_;
  }

  if (!variation_ids_header_copy.empty()) {
    headers->SetHeaderIfMissing("X-Chrome-Variations",
                                variation_ids_header_copy);
  }
}

VariationsHttpHeaderProvider::VariationsHttpHeaderProvider()
    : variation_ids_cache_initialized_(false) {
}

VariationsHttpHeaderProvider::~VariationsHttpHeaderProvider() {
}

void VariationsHttpHeaderProvider::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  VariationID new_id =
      GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_name, group_name);
  if (new_id == EMPTY_ID)
    return;

  base::AutoLock scoped_lock(lock_);
  variation_ids_set_.insert(new_id);
  UpdateVariationIDsHeaderValue();
}

void VariationsHttpHeaderProvider::InitVariationIDsCacheIfNeeded() {
  base::AutoLock scoped_lock(lock_);
  if (variation_ids_cache_initialized_)
    return;

  // Register for additional cache updates. This is done first to avoid a race
  // that could cause registered FieldTrials to be missed.
  DCHECK(MessageLoop::current());
  base::FieldTrialList::AddObserver(this);

  base::FieldTrial::ActiveGroups initial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&initial_groups);
  for (base::FieldTrial::ActiveGroups::const_iterator it =
           initial_groups.begin();
       it != initial_groups.end(); ++it) {
    const VariationID id =
        GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, it->trial_name,
                             it->group_name);
    if (id != EMPTY_ID)
      variation_ids_set_.insert(id);
  }
  UpdateVariationIDsHeaderValue();

  variation_ids_cache_initialized_ = true;
}

void VariationsHttpHeaderProvider::UpdateVariationIDsHeaderValue() {
  lock_.AssertAcquired();

  // The header value is a serialized protobuffer of Variation IDs which is
  // base64 encoded before transmitting as a string.
  if (variation_ids_set_.empty())
    return;

  // This is the bottleneck for the creation of the header, so validate the size
  // here. Force a hard maximum on the ID count in case the Variations server
  // returns too many IDs and DOSs receiving servers with large requests.
  DCHECK_LE(variation_ids_set_.size(), 10U);
  if (variation_ids_set_.size() > 20) {
    variation_ids_header_.clear();
    return;
  }

  metrics::ChromeVariations proto;
  for (std::set<VariationID>::const_iterator it = variation_ids_set_.begin();
       it != variation_ids_set_.end(); ++it) {
    proto.add_variation_id(*it);
  }

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string hashed;
  if (base::Base64Encode(serialized, &hashed)) {
    // If successful, swap the header value with the new one.
    // Note that the list of IDs and the header could be temporarily out of sync
    // if IDs are added as the header is recreated. The receiving servers are OK
    // with such descrepancies.
    variation_ids_header_ = hashed;
  } else {
    NOTREACHED() << "Failed to base64 encode Variation IDs value: "
                 << serialized;
  }
}

}  // namespace chrome_variations
