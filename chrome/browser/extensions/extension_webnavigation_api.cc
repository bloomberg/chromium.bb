// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/extension_webnavigation_api.h"

#include "base/json/json_writer.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_errors.h"

namespace keys = extension_webnavigation_api_constants;

namespace {

// Returns 0 if the navigation happens in the main frame, or the frame ID
// modulo 32 bits otherwise.
int GetFrameId(ProvisionalLoadDetails* details) {
  return details->main_frame() ? 0 : static_cast<int>(details->frame_id());
}

// Returns |time| as milliseconds since the epoch.
double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

}  // namespace

FrameNavigationState::FrameNavigationState() {
}

FrameNavigationState::~FrameNavigationState() {
}

bool FrameNavigationState::CanSendEvents(long long frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return frame_state != frame_state_map_.end() &&
      !frame_state->second.error_occurred;
}

void FrameNavigationState::TrackFrame(long long frame_id,
                                      const GURL& url,
                                      bool is_main_frame,
                                      const TabContents* tab_contents) {
  if (is_main_frame)
    RemoveTabContentsState(tab_contents);
  tab_contents_map_.insert(
      TabContentsToFrameIdMap::value_type(tab_contents, frame_id));
  FrameState& frame_state = frame_state_map_[frame_id];
  frame_state.error_occurred = (url.spec() == chrome::kUnreachableWebDataURL);
  frame_state.url = url;
  frame_state.is_main_frame = is_main_frame;
}

GURL FrameNavigationState::GetUrl(long long frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return GURL();
  }
  return frame_state->second.url;
}

bool FrameNavigationState::IsMainFrame(long long frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return false;
  }
  return frame_state->second.is_main_frame;
}

void FrameNavigationState::ErrorOccurredInFrame(long long frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].error_occurred = true;
}

void FrameNavigationState::RemoveTabContentsState(
    const TabContents* tab_contents) {
  typedef TabContentsToFrameIdMap::iterator FrameIdIterator;
  std::pair<FrameIdIterator, FrameIdIterator> frame_ids =
          tab_contents_map_.equal_range(tab_contents);
  for (FrameIdIterator frame_id = frame_ids.first; frame_id != frame_ids.second;
       ++frame_id) {
    frame_state_map_.erase(frame_id->second);
  }
  tab_contents_map_.erase(tab_contents);
}


// static
ExtensionWebNavigationEventRouter*
ExtensionWebNavigationEventRouter::GetInstance() {
  return Singleton<ExtensionWebNavigationEventRouter>::get();
}

void ExtensionWebNavigationEventRouter::Init() {
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::FRAME_PROVISIONAL_LOAD_START,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::FRAME_PROVISIONAL_LOAD_COMMITTED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::FRAME_DOM_CONTENT_LOADED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::FRAME_DID_FINISH_LOAD,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   NotificationService::AllSources());
  }
}

void ExtensionWebNavigationEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::FRAME_PROVISIONAL_LOAD_START:
      FrameProvisionalLoadStart(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NotificationType::FRAME_PROVISIONAL_LOAD_COMMITTED:
      FrameProvisionalLoadCommitted(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NotificationType::FRAME_DOM_CONTENT_LOADED:
      FrameDomContentLoaded(
          Source<NavigationController>(source).ptr(),
          *Details<long long>(details).ptr());
      break;
    case NotificationType::FRAME_DID_FINISH_LOAD:
      FrameDidFinishLoad(
          Source<NavigationController>(source).ptr(),
          *Details<long long>(details).ptr());
      break;
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      FailProvisionalLoadWithError(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;

    case NotificationType::TAB_CONTENTS_DESTROYED:
      navigation_state_.RemoveTabContentsState(
          Source<TabContents>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}
void ExtensionWebNavigationEventRouter::FrameProvisionalLoadStart(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  navigation_state_.TrackFrame(details->frame_id(),
                               details->url(),
                               details->main_frame(),
                               controller->tab_contents());
  if (!navigation_state_.CanSendEvents(details->frame_id()))
    return;
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(details));
  dict->SetInteger(keys::kRequestIdKey, 0);
  dict->SetReal(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnBeforeNavigate, json_args);
}

void ExtensionWebNavigationEventRouter::FrameProvisionalLoadCommitted(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  if (!navigation_state_.CanSendEvents(details->frame_id()))
    return;
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(details));
  dict->SetString(keys::kTransitionTypeKey,
                  PageTransition::CoreTransitionString(
                      details->transition_type()));
  dict->SetString(keys::kTransitionQualifiersKey,
                  PageTransition::QualifierString(
                      details->transition_type()));
  dict->SetReal(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnCommitted, json_args);
}

void ExtensionWebNavigationEventRouter::FrameDomContentLoaded(
    NavigationController* controller, long long frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey, navigation_state_.GetUrl(frame_id).spec());
  dict->SetInteger(keys::kFrameIdKey, navigation_state_.IsMainFrame(frame_id) ?
      0 : static_cast<int>(frame_id));
  dict->SetReal(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnDOMContentLoaded, json_args);
}

void ExtensionWebNavigationEventRouter::FrameDidFinishLoad(
    NavigationController* controller, long long frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey, navigation_state_.GetUrl(frame_id).spec());
  dict->SetInteger(keys::kFrameIdKey, navigation_state_.IsMainFrame(frame_id) ?
      0 : static_cast<int>(frame_id));
  dict->SetReal(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnCompleted, json_args);
}

void ExtensionWebNavigationEventRouter::FailProvisionalLoadWithError(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  if (!navigation_state_.CanSendEvents(details->frame_id()))
    return;
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(details));
  dict->SetString(keys::kErrorKey,
                  std::string(net::ErrorToString(details->error_code())));
  dict->SetReal(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  navigation_state_.ErrorOccurredInFrame(details->frame_id());
  DispatchEvent(controller->profile(), keys::kOnErrorOccurred, json_args);
}

void ExtensionWebNavigationEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    const std::string& json_args) {
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL());
  }
}
