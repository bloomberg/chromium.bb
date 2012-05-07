// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations_service.h"

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/proto/trials_seed.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

namespace {

// Default server of Variations seed info.
const char kDefaultVariationsServer[] =
    "https://clients4.google.com/chrome-variations/seed";

}  // namespace

// Static
VariationsService* VariationsService::GetInstance() {
  return Singleton<VariationsService>::get();
}

void VariationsService::LoadVariationsSeed(PrefService* local_prefs) {
  std::string base64_seed_data = local_prefs->GetString(prefs::kVariationsSeed);
  std::string seed_data;

  // If the decode process fails, assume the pref value is corrupt, and clear
  // it.
  if (!base::Base64Decode(base64_seed_data, &seed_data) ||
      !variations_seed_.ParseFromString(seed_data)) {
    VLOG(1) << "Variations Seed data in local pref is corrupt, clearing the "
            << "pref.";
    local_prefs->ClearPref(prefs::kVariationsSeed);
  }
}

void VariationsService::StartFetchingVariationsSeed() {
  pending_seed_request_.reset(content::URLFetcher::Create(
      GURL(kDefaultVariationsServer), content::URLFetcher::GET, this));
  pending_seed_request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES);
  pending_seed_request_->SetRequestContext(
      g_browser_process->system_request_context());
  pending_seed_request_->SetMaxRetries(5);
  pending_seed_request_->Start();
}

void VariationsService::OnURLFetchComplete(const content::URLFetcher* source) {
  DCHECK_EQ(pending_seed_request_.get(), source);
  // When we're done handling the request, the fetcher will be deleted.
  scoped_ptr<const content::URLFetcher> request(
      pending_seed_request_.release());
  if (request->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      request->GetResponseCode() != 200)
    return;

  std::string seed_data;
  request->GetResponseAsString(&seed_data);

  StoreSeedData(seed_data, g_browser_process->local_state());
}

// static
void VariationsService::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kVariationsSeed, std::string());
}

void VariationsService::StoreSeedData(const std::string& seed_data,
                                      PrefService* local_prefs) {
  // Only store the seed data if it parses correctly.
  chrome_variations::TrialsSeed seed;
  if (!seed.ParseFromString(seed_data)) {
    VLOG(1) << "Variations Seed data from server is not in valid proto format, "
            << "rejecting the seed.";
    return;
  }
  std::string base64_seed_data;
  if (!base::Base64Encode(seed_data, &base64_seed_data)) {
    VLOG(1) << "Variations Seed data from server fails Base64Encode, rejecting "
            << "the seed.";
    return;
  }
  local_prefs->SetString(prefs::kVariationsSeed, base64_seed_data);
}

VariationsService::VariationsService() {}
VariationsService::~VariationsService() {}
