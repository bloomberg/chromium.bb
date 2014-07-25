// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_http_header_provider.h"

#include <vector>

#include "base/base64.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/google/core/browser/google_util.h"
#include "components/variations/proto/client_variations.pb.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

// TODO(mathp): Once the move to variations namespace is complete, remove these.
using chrome_variations::EMPTY_ID;
using chrome_variations::GOOGLE_WEB_PROPERTIES;
using chrome_variations::GOOGLE_WEB_PROPERTIES_TRIGGER;
using chrome_variations::VariationID;

namespace variations {

namespace {

const char* kSuffixesToSetHeadersFor[] = {
  ".android.com",
  ".doubleclick.com",
  ".doubleclick.net",
  ".ggpht.com",
  ".googleadservices.com",
  ".googleapis.com",
  ".googlesyndication.com",
  ".googleusercontent.com",
  ".googlevideo.com",
  ".gstatic.com",
  ".ytimg.com",
};

}  // namespace

VariationsHttpHeaderProvider* VariationsHttpHeaderProvider::GetInstance() {
  return Singleton<VariationsHttpHeaderProvider>::get();
}

void VariationsHttpHeaderProvider::AppendHeaders(
    const GURL& url,
    bool incognito,
    bool uma_enabled,
    net::HttpRequestHeaders* headers) {
  // Note the criteria for attaching client experiment headers:
  // 1. We only transmit to Google owned domains which can evaluate experiments.
  //    1a. These include hosts which have a standard postfix such as:
  //         *.doubleclick.net or *.googlesyndication.com or
  //         exactly www.googleadservices.com or
  //         international TLD domains *.google.<TLD> or *.youtube.<TLD>.
  // 2. Only transmit for non-Incognito profiles.
  // 3. For the X-Chrome-UMA-Enabled bit, only set it if UMA is in fact enabled
  //    for this install of Chrome.
  // 4. For the X-Client-Data header, only include non-empty variation IDs.
  if (incognito || !ShouldAppendHeaders(url))
    return;

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
    // Note that prior to M33 this header was named X-Chrome-Variations.
    headers->SetHeaderIfMissing("X-Client-Data",
                                variation_ids_header_copy);
  }
}

bool VariationsHttpHeaderProvider::SetDefaultVariationIds(
    const std::string& variation_ids) {
  default_variation_ids_set_.clear();
  default_trigger_id_set_.clear();
  std::vector<std::string> entries;
  base::SplitString(variation_ids, ',', &entries);
  for (std::vector<std::string>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (it->empty()) {
      default_variation_ids_set_.clear();
      default_trigger_id_set_.clear();
      return false;
    }
    bool trigger_id = StartsWithASCII(*it, "t", true);
    // Remove the "t" prefix if it's there.
    std::string entry = trigger_id ? it->substr(1) : *it;

    int variation_id = 0;
    if (!base::StringToInt(entry, &variation_id)) {
      default_variation_ids_set_.clear();
      default_trigger_id_set_.clear();
      return false;
    }
    if (trigger_id)
      default_trigger_id_set_.insert(variation_id);
    else
      default_variation_ids_set_.insert(variation_id);
  }
  return true;
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
  VariationID new_trigger_id = GetGoogleVariationID(
      GOOGLE_WEB_PROPERTIES_TRIGGER, trial_name, group_name);
  if (new_id == EMPTY_ID && new_trigger_id == EMPTY_ID)
    return;

  base::AutoLock scoped_lock(lock_);
  if (new_id != EMPTY_ID)
    variation_ids_set_.insert(new_id);
  if (new_trigger_id != EMPTY_ID)
    variation_trigger_ids_set_.insert(new_trigger_id);

  UpdateVariationIDsHeaderValue();
}

void VariationsHttpHeaderProvider::InitVariationIDsCacheIfNeeded() {
  base::AutoLock scoped_lock(lock_);
  if (variation_ids_cache_initialized_)
    return;

  // Register for additional cache updates. This is done first to avoid a race
  // that could cause registered FieldTrials to be missed.
  DCHECK(base::MessageLoop::current());
  base::FieldTrialList::AddObserver(this);

  base::TimeTicks before_time = base::TimeTicks::Now();

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

    const VariationID trigger_id =
        GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_TRIGGER, it->trial_name,
                             it->group_name);
    if (trigger_id != EMPTY_ID)
      variation_trigger_ids_set_.insert(trigger_id);
  }
  UpdateVariationIDsHeaderValue();

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Variations.HeaderConstructionTime",
      (base::TimeTicks::Now() - before_time).InMicroseconds(),
      0,
      base::TimeDelta::FromSeconds(1).InMicroseconds(),
      50);

  variation_ids_cache_initialized_ = true;
}

void VariationsHttpHeaderProvider::UpdateVariationIDsHeaderValue() {
  lock_.AssertAcquired();

  // The header value is a serialized protobuffer of Variation IDs which is
  // base64 encoded before transmitting as a string.
  variation_ids_header_.clear();

  if (variation_ids_set_.empty() && default_variation_ids_set_.empty() &&
      variation_trigger_ids_set_.empty() && default_trigger_id_set_.empty()) {
    return;
  }

  // This is the bottleneck for the creation of the header, so validate the size
  // here. Force a hard maximum on the ID count in case the Variations server
  // returns too many IDs and DOSs receiving servers with large requests.
  const size_t total_id_count =
      variation_ids_set_.size() + variation_trigger_ids_set_.size();
  DCHECK_LE(total_id_count, 10U);
  if (total_id_count > 20)
    return;

  // Merge the two sets of experiment ids.
  std::set<VariationID> all_variation_ids_set = default_variation_ids_set_;
  for (std::set<VariationID>::const_iterator it = variation_ids_set_.begin();
       it != variation_ids_set_.end(); ++it) {
    all_variation_ids_set.insert(*it);
  }
  ClientVariations proto;
  for (std::set<VariationID>::const_iterator it = all_variation_ids_set.begin();
       it != all_variation_ids_set.end(); ++it) {
    proto.add_variation_id(*it);
  }

  std::set<VariationID> all_trigger_ids_set = default_trigger_id_set_;
  for (std::set<VariationID>::const_iterator it =
           variation_trigger_ids_set_.begin();
       it != variation_trigger_ids_set_.end(); ++it) {
    all_trigger_ids_set.insert(*it);
  }
  for (std::set<VariationID>::const_iterator it = all_trigger_ids_set.begin();
       it != all_trigger_ids_set.end(); ++it) {
    proto.add_trigger_variation_id(*it);
  }

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string hashed;
  base::Base64Encode(serialized, &hashed);
  // If successful, swap the header value with the new one.
  // Note that the list of IDs and the header could be temporarily out of sync
  // if IDs are added as the header is recreated. The receiving servers are OK
  // with such discrepancies.
  variation_ids_header_ = hashed;
}

// static
bool VariationsHttpHeaderProvider::ShouldAppendHeaders(const GURL& url) {
  if (google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    return true;
  }

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return false;

  // Some domains don't have international TLD extensions, so testing for them
  // is very straight forward.
  const std::string host = url.host();
  for (size_t i = 0; i < arraysize(kSuffixesToSetHeadersFor); ++i) {
    if (EndsWith(host, kSuffixesToSetHeadersFor[i], false))
      return true;
  }

  return google_util::IsYoutubeDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                         google_util::ALLOW_NON_STANDARD_PORTS);
}

}  // namespace variations
