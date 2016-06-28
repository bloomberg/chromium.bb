// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"

#include "base/metrics/histogram.h"

namespace content_settings {

void RecordMixedScriptAction(MixedScriptAction action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.MixedScript", action,
                            MIXED_SCRIPT_ACTION_COUNT);
}

}  // namespace content_settings
