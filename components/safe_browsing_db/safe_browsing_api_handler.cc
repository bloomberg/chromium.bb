// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/safe_browsing_db/safe_browsing_api_handler.h"

namespace safe_browsing {

namespace {

// TODO(nparker): Remove this as part of crbug/589610.
void OnRequestDoneShim(
    const SafeBrowsingApiHandler::URLCheckCallbackMeta& callback,
    SBThreatType sb_threat_type,
    const std::string& raw_metadata) {
  ThreatMetadata metadata_struct;
  metadata_struct.raw_metadata = raw_metadata;
  callback.Run(sb_threat_type, metadata_struct);
}

}  // namespace

SafeBrowsingApiHandler* SafeBrowsingApiHandler::instance_ = NULL;

// static
void SafeBrowsingApiHandler::SetInstance(SafeBrowsingApiHandler* instance) {
  instance_ = instance;
}

// static
SafeBrowsingApiHandler* SafeBrowsingApiHandler::GetInstance() {
  return instance_;
}

// TODO(nparker): Remove this as part of crbug/589610.
// Default impl since clank/ code doesn't yet support this.
void SafeBrowsingApiHandler::StartURLCheck(
    const URLCheckCallbackMeta& callback,
    const GURL& url,
    const std::vector<SBThreatType>& threat_types) {
  URLCheckCallback impl_callback = base::Bind(OnRequestDoneShim, callback);
  StartURLCheck(impl_callback, url, threat_types);
}

}  // namespace safe_browsing
