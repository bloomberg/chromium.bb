// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"

#include "base/mac/bind_objc_block.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Script to access the canonical URL from a web page.
char kCanonicalURLScript[] =
    "(function() {"
    "  var linkNode = document.querySelector(\"link[rel='canonical']\");"
    "  return linkNode ? linkNode.getAttribute(\"href\") : \"\";"
    "})()";

// Converts a |value| to a GURL. Returns an empty GURL if |value| is not a valid
// URL.
GURL UrlFromValue(const base::Value* value) {
  GURL canonical_url;
  if (value && value->is_string()) {
    canonical_url = GURL(value->GetString());
  }
  return canonical_url.is_valid() ? canonical_url : GURL::EmptyGURL();
}
}  // namespace

namespace activity_services {
void RetrieveCanonicalUrl(web::WebState* web_state,
                          ProceduralBlockWithURL completion) {
  // Do not use the canonical URL if the page is not secured with HTTPS.
  if (!web_state->GetVisibleURL().SchemeIsCryptographic()) {
    completion(GURL::EmptyGURL());
    return;
  }

  void (^javascript_completion)(const base::Value*) =
      ^(const base::Value* value) {
        GURL canonical_url = UrlFromValue(value);

        // If the canonical URL is not HTTPS, pass an empty GURL so that there
        // is not a downgrade from the HTTPS visible URL.
        completion(canonical_url.SchemeIsCryptographic() ? canonical_url
                                                         : GURL::EmptyGURL());
      };

  web_state->ExecuteJavaScript(base::UTF8ToUTF16(kCanonicalURLScript),
                               base::BindBlockArc(javascript_completion));
}
}  // namespace activity_services
