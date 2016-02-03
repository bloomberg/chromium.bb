// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_debug_log.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace banners {

const char kRendererRequestCancel[] =
    "renderer has requested the banner prompt be cancelled";
const char kManifestEmpty[] =
    "manifest could not be fetched, is empty, or could not be parsed";
const char kNoManifest[] = "site has no manifest <link> URL";
const char kCannotDetermineBestIcon[] =
    "could not determine the best icon to use";
const char kNoMatchingServiceWorker[] =
    "no matching service worker detected. You may need to reload the page, or "
    "check that the service worker for the current page also controls the "
    "start URL from the manifest";
const char kNoIconAvailable[] = "no icon available to display";
const char kUserNavigatedBeforeBannerShown[] =
    "the user navigated before the banner could be shown";
const char kStartURLNotValid[] = "start URL in manifest is not valid";
const char kManifestMissingNameOrShortName[] =
    "one of manifest name or short name must be specified";
const char kManifestMissingSuitableIcon[] =
    "manifest does not contain a suitable icon - PNG format of at least "
    "144x144px is required, and the sizes attribute must be set";
const char kNotLoadedInMainFrame[] = "page not loaded in the main frame";
const char kNotServedFromSecureOrigin[] =
    "page not served from a secure origin";
// The leading space is intentional as another string is prepended.
const char kIgnoredNotSupportedOnAndroid[] =
    " application ignored: not supported on Android";
const char kIgnoredNoId[] = "play application ignored: no id provided";
const char kIgnoredIdsDoNotMatch[] =
    "play application ignored: app URL and id fields were specified in the "
    "manifest, but they do not match";

void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    const std::string& message,
                                    bool is_debug_mode) {
  OutputDeveloperDebugMessage(web_contents, "not shown: " + message,
                              is_debug_mode);
}

void OutputDeveloperDebugMessage(content::WebContents* web_contents,
                                 const std::string& message,
                                 bool is_debug_mode) {
  if (!is_debug_mode || !web_contents)
    return;
  web_contents->GetMainFrame()->AddMessageToConsole(
      content::CONSOLE_MESSAGE_LEVEL_DEBUG, "App banner " + message);
}

}  // namespace banners
