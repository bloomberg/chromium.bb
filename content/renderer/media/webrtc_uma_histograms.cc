// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_uma_histograms.h"

#include <set>

#include "base/lazy_instance.h"

namespace content {

namespace {

typedef std::set<base::string16> OriginSet;
base::LazyInstance<OriginSet>::Leaky counted_security_origins;

}  // namespace

void UpdateWebRTCMethodCount(JavaScriptAPIName api_name) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCount", api_name, INVALID_NAME);
}

void UpdateWebRTCUniqueOriginMethodCount(
    JavaScriptAPIName api_name,
    const base::string16& security_origin) {
  OriginSet* origins = counted_security_origins.Pointer();

  if (origins->find(security_origin) == origins->end()) {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCountUniqueByOrigin",
                              api_name, INVALID_NAME);
    origins->insert(security_origin);
  }
}

} //  namespace content
