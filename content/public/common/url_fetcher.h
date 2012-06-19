// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#pragma once

#include "content/common/content_export.h"

class GURL;

namespace net {
class URLFetcher;
}  // namespace

namespace content {

// Mark URLRequests started by the URLFetcher to stem from the given render
// view.
CONTENT_EXPORT void AssociateURLFetcherWithRenderView(
    net::URLFetcher* url_fetcher,
    const GURL& first_party_for_cookies,
    int render_process_id,
    int render_view_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
