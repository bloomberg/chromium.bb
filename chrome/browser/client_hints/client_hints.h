// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>

class GURL;

namespace content {
class BrowserContext;
}

namespace net {
class HttpRequestHeaders;
}

namespace client_hints {

// Allow the embedder to return additional headers related to client hints that
// should be sent when fetching |url|. May return a nullptr.
std::unique_ptr<net::HttpRequestHeaders>
GetAdditionalNavigationRequestClientHintsHeaders(
    content::BrowserContext* context,
    const GURL& url);

}  // namespace client_hints

#endif  // CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
