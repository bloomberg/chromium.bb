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
extern const char kRendererRequestCancel[];
extern const char kManifestEmpty[];
extern const char kNoManifest[];
extern const char kCannotDetermineBestIcon[];
extern const char kNoMatchingServiceWorker[];
extern const char kNoIconAvailable[];
extern const char kBannerAlreadyAdded[];
extern const char kUserNavigatedBeforeBannerShown[];
extern const char kStartURLNotValid[];
extern const char kManifestMissingNameOrShortName[];
extern const char kManifestMissingSuitableIcon[];
extern const char kNotLoadedInMainFrame[];
extern const char kNotServedFromSecureOrigin[];
extern const char kIgnoredNotSupportedOnAndroid[];
extern const char kIgnoredNoId[];
extern const char kIgnoredIdsDoNotMatch[];

// Logs a message to the main console if a banner could not be shown
// and the engagement checks have been bypassed.
void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    const std::string& message,
                                    bool is_debug_mode);

// Logs a debugging message to the main console if the engagement checks have
// been bypassed.
void OutputDeveloperDebugMessage(content::WebContents* web_contents,
                                 const std::string& message,
                                 bool is_debug_mode);

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_DEBUG_LOG_H_
