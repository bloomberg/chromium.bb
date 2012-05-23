// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/load_from_memory_cache_details.h"

LoadFromMemoryCacheDetails::LoadFromMemoryCacheDetails(
    const GURL& url,
    int pid,
    int cert_id,
    net::CertStatus cert_status,
    std::string http_method,
    std::string mime_type,
    ResourceType::Type resource_type)
    : url_(url),
      pid_(pid),
      cert_id_(cert_id),
      cert_status_(cert_status),
      http_method_(http_method),
      mime_type_(mime_type),
      resource_type_(resource_type) {
}

LoadFromMemoryCacheDetails::~LoadFromMemoryCacheDetails() {
}
