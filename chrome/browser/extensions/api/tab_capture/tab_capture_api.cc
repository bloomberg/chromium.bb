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
#include "chrome/browser/extensions/extension_tab_id_map.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/feature_switch.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

namespace TabCapture = extensions::api::tab_capture;
namespace GetCapturedTabs = TabCapture::GetCapturedTabs;

namespace extensions {

namespace {

const char kPermissionError[] =
    "Extension does not have permission for tab capture.";
const char kErrorTabIdNotFound[] = "Could not find the specified tab.";
const char kCapturingSameTab[] = "Cannot capture a tab with an active stream.";

// Keys/values for media stream constraints.
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId";
const char kMediaStreamSourceTab[] = "tab";

}  // namespace

bool TabCaptureCaptureFunction::RunImpl() {
  if (!GetExtension()->HasAPIPermission(APIPermission::kTabCapture) ||
      !FeatureSwitch::tab_capture()->IsEnabled()) {
    error_ = kPermissionError;
    return false;
  }

  scoped_ptr<api::tab_capture::Capture::Params> params =
      TabCapture::Capture::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int render_process_id = -1;
  int routing_id = -1;
  int tab_id = *params->tab_id.get();
  bool found_tab = false;

  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    Browser* target_browser = *iter;
    TabStripModel* target_tab_strip = target_browser->tab_strip_model();

    for (int i = 0; i < target_tab_strip->count(); ++i) {
      TabContents* target_contents = target_tab_strip->GetTabContentsAt(i);
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(target_contents->web_contents());
      if (session_tab_helper->session_id().id() == tab_id) {
        found_tab = true;
        content::RenderViewHost* const rvh =
            target_contents->web_contents()->GetRenderViewHost();
        routing_id = rvh->GetRoutingID();
        render_process_id = rvh->GetProcess()->GetID();
        break;
      }
    }
  }

  if (!found_tab) {
    error_ = kErrorTabIdNotFound;
    SetResult(base::Value::CreateIntegerValue(0));
    return false;
  }

  std::string device_id =
      base::StringPrintf("%i:%i", render_process_id, routing_id);

  bool audio = false;
  bool video = false;
  if (params->options.get()) {
    if (params->options->audio.get())
      audio = *params->options->audio.get();
    if (params->options->video.get())
      video = *params->options->video.get();
  }

  base::DictionaryValue* result = new base::DictionaryValue();
  if (video) {
    result->SetString(std::string("videoConstraints.mandatory.") +
        kMediaStreamSource, kMediaStreamSourceTab);
    result->SetString(std::string("videoConstraints.mandatory.") +
        kMediaStreamSourceId, device_id);
  } else {
    result->SetBoolean(std::string("videoConstraints"), false);
  }

  if (audio) {
    result->SetString(std::string("audioConstraints.mandatory.") +
        kMediaStreamSource, kMediaStreamSourceTab);
    result->SetString(std::string("audioConstraints.mandatory.") +
        kMediaStreamSourceId, device_id);
  } else {
    result->SetBoolean(std::string("audioConstraints"), false);
  }

  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile());
  if (!registry->AddRequest(
          std::make_pair(render_process_id, routing_id),
          TabCaptureRegistry::TabCaptureRequest(
              GetExtension()->id(), tab_id,
              tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_NONE))) {
    error_ = kCapturingSameTab;
    SetResult(base::Value::CreateIntegerValue(0));
    return false;
  }

  SetResult(result);
  return true;
}

bool TabCaptureGetCapturedTabsFunction::RunImpl() {
  if (!GetExtension()->HasAPIPermission(APIPermission::kTabCapture) ||
      !FeatureSwitch::tab_capture()->IsEnabled()) {
    error_ = kPermissionError;
    return false;
  }

  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile());

  const TabCaptureRegistry::CaptureRequestList& captured_tabs =
      registry->GetCapturedTabs(GetExtension()->id());

  base::ListValue *list = new base::ListValue();
  for (TabCaptureRegistry::CaptureRequestList::const_iterator it =
       captured_tabs.begin(); it != captured_tabs.end(); it++) {
    scoped_ptr<tab_capture::CaptureInfo> info(new tab_capture::CaptureInfo());
    info->tab_id = it->tab_id;
    info->status = it->status;
    list->Append(info->ToValue().release());
  }

  SetResult(list);
  return true;
}

}  // namespace extensions
