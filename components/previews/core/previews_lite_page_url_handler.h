// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_LITE_PAGE_URL_HANDLER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_LITE_PAGE_URL_HANDLER_H_

#include <string>

#include "url/gurl.h"

namespace previews {

// Returns true if the given |url| has the same domain as the lite page previews
// server.
bool IsLitePageRedirectPreviewDomain(const GURL& url);

// Returns true if the given URL is a Lite Page Preview URL. This does more
// checking than |IsLitePageRedirectPreviewDomain| so be sure to use the right
// one.
bool IsLitePageRedirectPreviewURL(const GURL& url);

// Attempts to extract the original URL from the given Previews URL. Returns
// false if |url| is not a valid Preview URL.
bool ExtractOriginalURLFromLitePageRedirectURL(const GURL& url,
                                               std::string* original_url);

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_LITE_PAGE_URL_HANDLER_H_
