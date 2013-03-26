// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_RISK_FINGERPRINT_H_
#define COMPONENTS_AUTOFILL_BROWSER_RISK_FINGERPRINT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "components/autofill/browser/autofill_manager_delegate.h"

class PrefService;

namespace base {
class Time;
}

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
// |version| is the version number of the application.
// |charset| is the default character set.
// |accept_languages| is the Accept-Languages setting.
// |install_time| is the absolute time of installation.
void GetFingerprint(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const content::WebContents& web_contents,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    const base::Time& install_time,
    DialogType dialog_type,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback);

// Exposed for testing:
namespace internal {

void GetFingerprintInternal(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const WebKit::WebScreenInfo& screen_info,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    const base::Time& install_time,
    DialogType dialog_type,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback);

}  // namespace internal

}  // namespace risk
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_RISK_FINGERPRINT_H_
