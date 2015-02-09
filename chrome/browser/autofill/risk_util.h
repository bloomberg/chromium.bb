// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_RISK_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_RISK_UTIL_H_

#include <stdint.h>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace content {
class WebContents;
}

namespace autofill {

void LoadRiskData(uint64 obfuscated_gaia_id,
                  content::WebContents* web_contents,
                  const base::Callback<void(const std::string&)>& callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_RISK_UTIL_H_
