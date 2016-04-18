// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_DEBUG_LOG_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_DEBUG_LOG_H_

#include <string>

namespace content {
class WebContents;
}  // namespace content

namespace banners {

// Error message strings used to notify developers via the console.

enum OutputDeveloperMessageCode {
  kRendererRequestCancel,
  kManifestEmpty,
  kNoManifest,
  kNoIconMatchingRequirements,
  kCannotDownloadIcon,
  kNoMatchingServiceWorker,
  kNoIconAvailable,
  kUserNavigatedBeforeBannerShown,
  kStartURLNotValid,
  kManifestDisplayStandaloneFullscreen,
  kManifestMissingNameOrShortName,
  kManifestMissingSuitableIcon,
  kNotLoadedInMainFrame,
  kNotServedFromSecureOrigin,
  kIgnoredNotSupportedOnAndroid,
  kIgnoredNoId,
  kIgnoredIdsDoNotMatch,
};

// Logs a message to the main console if a banner could not be shown
// and the engagement checks have been bypassed.
void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    OutputDeveloperMessageCode code,
                                    bool is_debug_mode);

// Logs a message to the main console if a banner could not be shown
// and the engagement checks have been bypassed.
void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    OutputDeveloperMessageCode code,
                                    const std::string& param,
                                    bool is_debug_mode);

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_DEBUG_LOG_H_
