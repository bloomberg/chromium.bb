// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Tab Capture API.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

using extensions::api::tab_capture::MediaStreamConstraint;

namespace TabCapture = extensions::api::tab_capture;
namespace GetCapturedTabs = TabCapture::GetCapturedTabs;

namespace extensions {

namespace {

const char kCapturingSameTab[] = "Cannot capture a tab with an active stream.";
const char kFindingTabError[] = "Error finding tab to capture.";
const char kNoAudioOrVideo[] = "Capture failed. No audio or video requested.";
const char kGrantError[] =
    "Extension has not been invoked for the current page (see activeTab "
    "permission). Chrome pages cannot be captured.";

// Keys/values for media stream constraints.
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId";
const char kMediaStreamSourceTab[] = "tab";

// Whitelisted extensions that do not check for a browser action grant because
// they provide API's.
const char* whitelisted_extensions[] = {
  "enhhojjnijigcajfphajepfemndkmdlo",  // Dev
  "pkedcjkdefgpdelpbcmbmeomcjbeemfm",  // Trusted Tester
  "fmfcbgogabcbclcofgocippekhfcmgfj",  // Staging
  "hfaagokkkhdbgiakmmlclaapfelnkoah",  // Canary
  "F155646B5D1CA545F7E1E4E20D573DFDD44C2540",  // Trusted Tester (public)
  "16CA7A47AAE4BE49B1E75A6B960C3875E945B264"   // Release
};

}  // namespace

bool TabCaptureCaptureFunction::RunSync() {
  scoped_ptr<api::tab_capture::Capture::Params> params =
      TabCapture::Capture::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Figure out the active WebContents and retrieve the needed ids.
  Browser* target_browser = chrome::FindAnyBrowser(
      GetProfile(), include_incognito(), chrome::GetActiveDesktop());
  if (!target_browser) {
    error_ = kFindingTabError;
    return false;
  }

  content::WebContents* target_contents =
      target_browser->tab_strip_model()->GetActiveWebContents();
  if (!target_contents) {
    error_ = kFindingTabError;
    return false;
  }

  const Extension* extension = GetExtension();
  const std::string& extension_id = extension->id();

  // Make sure either we have been granted permission to capture through an
  // extension icon click or our extension is whitelisted.
  if (!extension->permissions_data()->HasAPIPermissionForTab(
          SessionID::IdForTab(target_contents),
          APIPermission::kTabCaptureForTab) &&
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWhitelistedExtensionID) != extension_id &&
      !SimpleFeature::IsIdInList(
          extension_id,
          std::set<std::string>(
              whitelisted_extensions,
              whitelisted_extensions + arraysize(whitelisted_extensions)))) {
    error_ = kGrantError;
    return false;
  }

  // Create a constraints vector. We will modify all the constraints in this
  // vector to append our chrome specific constraints.
  std::vector<MediaStreamConstraint*> constraints;
  bool has_audio = params->options.audio.get() && *params->options.audio.get();
  bool has_video = params->options.video.get() && *params->options.video.get();

  if (!has_audio && !has_video) {
    error_ = kNoAudioOrVideo;
    return false;
  }

  if (has_audio) {
    if (!params->options.audio_constraints.get())
      params->options.audio_constraints.reset(new MediaStreamConstraint);

    constraints.push_back(params->options.audio_constraints.get());
  }
  if (has_video) {
    if (!params->options.video_constraints.get())
      params->options.video_constraints.reset(new MediaStreamConstraint);

    constraints.push_back(params->options.video_constraints.get());
  }

  // Device id we use for Tab Capture.
  content::RenderFrameHost* const main_frame = target_contents->GetMainFrame();
  // TODO(miu): We should instead use a "randomly generated device ID" scheme,
  // like that employed by the desktop capture API.  http://crbug.com/163100
  const std::string device_id = base::StringPrintf(
      "web-contents-media-stream://%i:%i",
      main_frame->GetProcess()->GetID(),
      main_frame->GetRoutingID());

  // Append chrome specific tab constraints.
  for (std::vector<MediaStreamConstraint*>::iterator it = constraints.begin();
       it != constraints.end(); ++it) {
    base::DictionaryValue* constraint = &(*it)->mandatory.additional_properties;
    constraint->SetString(kMediaStreamSource, kMediaStreamSourceTab);
    constraint->SetString(kMediaStreamSourceId, device_id);
  }

  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistry::Get(GetProfile());
  if (!registry->AddRequest(target_contents, extension_id)) {
    error_ = kCapturingSameTab;
    return false;
  }

  // Copy the result from our modified input parameters. This will be
  // intercepted by custom bindings which will build and send the special
  // WebRTC user media request.
  base::DictionaryValue* result = new base::DictionaryValue();
  result->MergeDictionary(params->options.ToValue().get());

  SetResult(result);
  return true;
}

bool TabCaptureGetCapturedTabsFunction::RunSync() {
  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistry::Get(GetProfile());
  base::ListValue* const list = new base::ListValue();
  if (registry)
    registry->GetCapturedTabs(GetExtension()->id(), list);
  SetResult(list);
  return true;
}

}  // namespace extensions
