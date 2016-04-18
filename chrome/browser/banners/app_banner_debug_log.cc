// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_debug_log.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace banners {

static const char kRendererRequestCancelMessage[] =
    "page has requested the banner prompt be cancelled";
static const char kManifestEmptyMessage[] =
    "manifest could not be fetched, is empty, or could not be parsed";
static const char kNoManifestMessage[] =
    "site has no manifest <link> URL";
static const char kNoIconMatchingRequirementsMessage[] =
    "%spx square icon is required, but no supplied icon is at least this size";
static const char kCannotDownloadIconMessage[] =
    "could not download the specified icon";
static const char kNoMatchingServiceWorkerMessage[] =
    "no matching service worker detected. You may need to reload the page, or "
    "check that the service worker for the current page also controls the "
    "start URL from the manifest";
static const char kNoIconAvailableMessage[] =
    "no icon available to display";
static const char kUserNavigatedBeforeBannerShownMessage[] =
    "the user navigated before the banner could be shown";
static const char kStartURLNotValidMessage[] =
    "start URL in manifest is not valid";
static const char kManifestDisplayStandaloneFullscreenMessage[] =
    "manifest display property must be set to 'standalone' or 'fullscreen'";
static const char kManifestMissingNameOrShortNameMessage[] =
    "one of manifest name or short name must be specified";
static const char kManifestMissingSuitableIconMessage[] =
    "manifest does not contain a suitable icon - PNG format of at least "
    "144x144px is required, and the sizes attribute must be set";
static const char kNotLoadedInMainFrameMessage[] =
    "page not loaded in the main frame";
static const char kNotServedFromSecureOriginMessage[] =
    "page not served from a secure origin";
// The leading space is intentional as another string is prepended.
static const char kIgnoredNotSupportedOnAndroidMessage[] =
    "%s application is not supported on Android";
static const char kIgnoredNoIdMessage[] =
    "no Play store ID provided";
static const char kIgnoredIdsDoNotMatchMessage[] =
    "a Play app URL and Play store ID were specified in the manifest, but they"
    " do not match";

void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    OutputDeveloperMessageCode code,
                                    bool is_debug_mode) {
  OutputDeveloperNotShownMessage(web_contents, code, std::string(),
                                 is_debug_mode);
}

void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    OutputDeveloperMessageCode code,
                                    const std::string& param,
                                    bool is_debug_mode) {
  if (!is_debug_mode || !web_contents)
    return;

  const char* pattern;
  content::ConsoleMessageLevel severity = content::CONSOLE_MESSAGE_LEVEL_ERROR;
  switch (code) {
    case kRendererRequestCancel:
      pattern = kRendererRequestCancelMessage;
      severity = content::CONSOLE_MESSAGE_LEVEL_LOG;
      break;
    case kManifestEmpty:
      pattern = kManifestEmptyMessage;
      break;
    case kNoManifest:
      pattern = kNoManifestMessage;
      break;
    case kNoIconMatchingRequirements:
      pattern = kNoIconMatchingRequirementsMessage;
      break;
    case kCannotDownloadIcon:
      pattern = kCannotDownloadIconMessage;
      break;
    case kNoMatchingServiceWorker:
      pattern = kNoMatchingServiceWorkerMessage;
      break;
    case kNoIconAvailable:
      pattern = kNoIconAvailableMessage;
      break;
    case kUserNavigatedBeforeBannerShown:
      pattern = kUserNavigatedBeforeBannerShownMessage;
      severity = content::CONSOLE_MESSAGE_LEVEL_WARNING;
      break;
    case kStartURLNotValid:
      pattern = kStartURLNotValidMessage;
      break;
    case kManifestDisplayStandaloneFullscreen:
      pattern = kManifestDisplayStandaloneFullscreenMessage;
      break;
    case kManifestMissingNameOrShortName:
      pattern = kManifestMissingNameOrShortNameMessage;
      break;
    case kManifestMissingSuitableIcon:
      pattern = kManifestMissingSuitableIconMessage;
      break;
    case kNotLoadedInMainFrame:
      pattern = kNotLoadedInMainFrameMessage;
      break;
    case kNotServedFromSecureOrigin:
      pattern = kNotServedFromSecureOriginMessage;
      break;
    case kIgnoredNotSupportedOnAndroid:
      pattern = kIgnoredNotSupportedOnAndroidMessage;
      severity = content::CONSOLE_MESSAGE_LEVEL_WARNING;
      break;
    case kIgnoredNoId:
      pattern = kIgnoredNoIdMessage;
      break;
    case kIgnoredIdsDoNotMatch:
      pattern = kIgnoredIdsDoNotMatchMessage;
      break;
    default:
      NOTREACHED();
      return;
  }
  std::string message = param.empty() ?
      pattern : base::StringPrintf(pattern, param.c_str());
  web_contents->GetMainFrame()->AddMessageToConsole(
      severity, "App banner not shown: " + message);

}

}  // namespace banners
