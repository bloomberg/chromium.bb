// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dns_prefetch/common/prefetch_common.h"

namespace dns_prefetch {

const size_t kMaxDnsHostnamesPerRequest = 30;
const size_t kMaxDnsHostnameLength = 255;

LookupRequest::LookupRequest() {
}

LookupRequest::~LookupRequest() {
}

}  // namespace dns_prefetch
