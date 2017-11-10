// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_CANONICAL_URL_RETRIEVER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_CANONICAL_URL_RETRIEVER_PRIVATE_H_

namespace activity_services {
// The histogram key to report the result of the Canonical URL retrieval.
const char kCanonicalURLResultHistogram[] = "IOS.CanonicalURLResult";

// Used to report the result of the retrieval of the Canonical URL.
enum CanonicalURLResult {
  // The canonical URL retrieval failed because the visible URL is not HTTPS.
  FAILED_VISIBLE_URL_NOT_HTTPS = 0,

  // The canonical URL retrieval failed because the canonical URL is not HTTPS
  // (but the visible URL is).
  FAILED_CANONICAL_URL_NOT_HTTPS,

  // The canonical URL retrieval failed because the page did not define one.
  FAILED_NO_CANONICAL_URL_DEFINED,

  // The canonical URL retrieval failed because the page's canonical URL was
  // invalid.
  FAILED_CANONICAL_URL_INVALID,

  // The canonical URL retrieval succeeded. The retrieved canonical URL is
  // different from the visible URL.
  SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE,

  // The canonical URL retrieval succeeded. The retrieved canonical URL is the
  // same as the visible URL.
  SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE,

  // The count of canonical URL results. This must be the last item in the enum.
  CANONICAL_URL_RESULT_COUNT
};
}  // namespace activity_services

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_CANONICAL_URL_RETRIEVER_PRIVATE_H_
