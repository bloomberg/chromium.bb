// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_request.h"

#include "content/public/common/appcache_info.h"

namespace content {

// kAppCacheNoHostId can't be used in the header, because appcache_info.h
// includes a webkit file with a Status enum, and X11 #defines Status.
ResourceRequest::ResourceRequest() : appcache_host_id(kAppCacheNoHostId) {}
ResourceRequest::ResourceRequest(const ResourceRequest& request) = default;
ResourceRequest::~ResourceRequest() {}

}  // namespace content
