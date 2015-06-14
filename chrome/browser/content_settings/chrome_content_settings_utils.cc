// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "components/rappor/rappor_utils.h"

namespace content_settings {

void RecordMixedScriptAction(MixedScriptAction action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.MixedScript", action,
                            MIXED_SCRIPT_ACTION_COUNT);
}

void RecordMixedScriptActionWithRAPPOR(MixedScriptAction action,
                                       const GURL& url) {
  std::string metric;
  switch (action) {
    case MIXED_SCRIPT_ACTION_DISPLAYED_SHIELD:
      metric = "ContentSettings.MixedScript.DisplayedShield";
      break;
    case MIXED_SCRIPT_ACTION_CLICKED_ALLOW:
      metric = "ContentSettings.MixedScript.UserClickedAllow";
      break;
    default:
      NOTREACHED();
  }

  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          metric,
                                          url);
}

}  // namespace content_settings
