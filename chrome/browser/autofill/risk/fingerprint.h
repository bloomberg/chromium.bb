// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_RISK_FINGERPRINT_H_
#define CHROME_BROWSER_AUTOFILL_RISK_FINGERPRINT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

class PrefService;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace WebKit {
struct WebScreenInfo;
}

namespace autofill {
namespace risk {

class Fingerprint;

// Asynchronously calls |callback| with statistics that, collectively, provide a
// unique fingerprint for this (machine, user) pair, used for fraud prevention.
// |gaia_id| should be the user id for Google's authentication system.
// |window_bounds| should be the bounds of the containing Chrome window.
// |web_contents| should be the host for the page the user is interacting with.
// |prefs| should reflect the active Chrome profile's user preferences.
void GetFingerprint(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const content::WebContents& web_contents,
    const PrefService& prefs,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback);

// Exposed for testing:
namespace internal {

void GetFingerprintInternal(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const WebKit::WebScreenInfo& screen_info,
    const PrefService& prefs,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback);

}  // namespace internal

}  // namespace risk
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_RISK_FINGERPRINT_H_
