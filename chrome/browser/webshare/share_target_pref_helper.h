// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SHARE_TARGET_PREF_HELPER_H_
#define CHROME_BROWSER_WEBSHARE_SHARE_TARGET_PREF_HELPER_H_

#include "base/optional.h"
#include "base/strings/string_piece.h"

class PrefService;

// Adds the Web Share target |share_target_origin| with template |url_template|
// to |pref_service| under kWebShareVisitedTargets. If |url_template| is null,
// this function will remove |share_target_origin| from kWebShareVisitedTargets,
// if it is there.
void UpdateShareTargetInPrefs(base::StringPiece manifest_url,
                           base::Optional<std::string> url_template,
                           PrefService* pref_service);

#endif // CHROME_BROWSER_WEBSHARE_SHARE_TARGET_PREF_HELPER_H_
