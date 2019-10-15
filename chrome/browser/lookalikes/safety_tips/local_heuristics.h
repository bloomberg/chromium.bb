// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_LOCAL_HEURISTICS_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_LOCAL_HEURISTICS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "url/gurl.h"

// These functions exist as utility functions, and are currently used in
// "safety_tip_heuristics". These functions SHOULD NOT be called directly. See
// reptuation_service.h for module use.
namespace safety_tips {

// Checks to see whether a given URL qualifies as a lookalike domain, and thus
// should trigger a safety tip. This algorithm factors in the sites that the
// user has already engaged with. This heuristic stores a "safe url" that the
// navigated domain is a lookalike to, in the passed |safe_url|.
bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const lookalikes::DomainInfo& navigated_domain,
    const std::vector<lookalikes::DomainInfo>& engaged_sites,
    GURL* safe_url);

// Checks to see whether a given URL contains sensitive keywords in a way
// that it should trigger a safety tip.
bool ShouldTriggerSafetyTipFromKeywordInURL(
    const GURL& url,
    const char* const sensitive_keywords[],
    size_t num_keywords);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_LOCAL_HEURISTICS_H_
