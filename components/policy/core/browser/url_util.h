// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_UTIL_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_UTIL_H_

#include "components/policy/policy_export.h"

class GURL;

namespace policy {
namespace url_util {

// Normalizes a URL for matching purposes.
POLICY_EXPORT GURL Normalize(const GURL& url);

// Helper function to extract the underlying URL wrapped by services such as
// Google AMP or Google Translate. Returns an empty GURL if |url| doesn't match
// a known format.
POLICY_EXPORT GURL GetEmbeddedURL(const GURL& url);

}  // namespace url_util
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_UTIL_H_
