// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Tab Capture API.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"

#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"
#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/feature_switch.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

using extensions::api::tab_capture::MediaStreamConstraint;

namespace TabCapture = extensions::api::tab_capture;
namespace GetCapturedTabs = TabCapture::GetCapturedTabs;

namespace extensions {

namespace {

const char kCapturingSameTab[] = "Cannot capture a tab with an active stream.";
const char kFindingTabError[] = "Error finding tab to capture.";
const char kNoAudioOrVideo[] = "Capture failed. No audio or video requested.";
const char kPermissionError[] = "Tab Capture API flag is not enabled.";

// Keys/values for media stream constraints.
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId";
const char kMediaStreamSourceTab[] = "tab";

}  // namespace

bool TabCaptureCaptureFunction::RunImpl() {
  if (!FeatureSwitch::tab_capture()->IsEnabled()) {
    error_ = kPermissionError;
    return false;
  }

  scoped_ptr<api::tab_capture::Capture::Params> params =
      TabCapture::Capture::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Figure out the active WebContents and retrieve the needed ids.
  Browser* target_browser = chrome::FindAnyBrowser(profile(),
                                                   include_incognito(),
                                                   chrome::GetActiveDesktop());
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

  content::RenderViewHost* const rvh = target_contents->GetRenderViewHost();
  int render_process_id = rvh->GetProcess()->GetID();
  int routing_id = rvh->GetRoutingID();
  int tab_id = SessionTabHelper::FromWebContents(target_contents)->
                   session_id().id();

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
  std::string device_id =
      base::StringPrintf("%i:%i", render_process_id, routing_id);

  // Append chrome specific tab constraints.
  for (std::vector<MediaStreamConstraint*>::iterator it = constraints.begin();
       it != constraints.end(); ++it) {
    base::DictionaryValue* constraint = &(*it)->mandatory.additional_properties;
    constraint->SetString(kMediaStreamSource, kMediaStreamSourceTab);
    constraint->SetString(kMediaStreamSourceId, device_id);
  }

  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile());
  if (!registry->AddRequest(
          std::make_pair(render_process_id, routing_id),
          TabCaptureRegistry::TabCaptureRequest(
              GetExtension()->id(), tab_id,
              tab_capture::TAB_CAPTURE_STATE_NONE))) {
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

bool TabCaptureGetCapturedTabsFunction::RunImpl() {
  if (!FeatureSwitch::tab_capture()->IsEnabled()) {
    error_ = kPermissionError;
    return false;
  }

  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile());

  const TabCaptureRegistry::CaptureRequestList& captured_tabs =
      registry->GetCapturedTabs(GetExtension()->id());

  base::ListValue *list = new base::ListValue();
  for (TabCaptureRegistry::CaptureRequestList::const_iterator it =
       captured_tabs.begin(); it != captured_tabs.end(); ++it) {
    scoped_ptr<tab_capture::CaptureInfo> info(new tab_capture::CaptureInfo());
    info->tab_id = it->tab_id;
    info->status = it->status;
    list->Append(info->ToValue().release());
  }

  SetResult(list);
  return true;
}

}  // namespace extensions
