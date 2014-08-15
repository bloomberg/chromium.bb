// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_

#include <string>

class GURL;

namespace dom_distiller {

namespace url_utils {

// Returns the URL for viewing distilled content for an entry.
const GURL GetDistillerViewUrlFromEntryId(const std::string& scheme,
                                          const std::string& entry_id);

// Returns the URL for viewing distilled content for a URL.
const GURL GetDistillerViewUrlFromUrl(const std::string& scheme,
                                      const GURL& view_url);

// Returns the value of the query parameter for the given |key| for a given URL.
// If the URL is invalid or if the key is not found, returns an empty string.
// If there are multiple keys found in the URL, returns the value for the first
// key.
std::string GetValueForKeyInUrl(const GURL& url, const std::string& key);

// Returns the value of the query parameter for the given path.
std::string GetValueForKeyInUrlPathQuery(const std::string& path,
                                         const std::string& key);

// Returns whether it should be possible to distill the given |url|.
bool IsUrlDistillable(const GURL& url);

// Returns whether the given |url| is for a distilled page.
bool IsDistilledPage(const GURL& url);

// Returns a JavaScript snippet that returns whether or not a page should be
// used with DomDistillerService and can be executed in a live page.
std::string GetIsDistillableJs();

}  // namespace url_utils

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_
