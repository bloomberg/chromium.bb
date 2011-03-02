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
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/provisional_load_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/net_errors.h"

namespace keys = extension_webnavigation_api_constants;

namespace {

// URL schemes for which we'll send events.
const char* kValidSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
};

// Returns 0 if the navigation happens in the main frame, or the frame ID
// modulo 32 bits otherwise.
int GetFrameId(ProvisionalLoadDetails* details) {
  return details->main_frame() ? 0 : static_cast<int>(details->frame_id());
}

// Returns |time| as milliseconds since the epoch.
double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

// Dispatches events to the extension message service.
void DispatchEvent(Profile* profile,
                   const char* event_name,
                   const std::string& json_args) {
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL());
  }
}

// Constructs and dispatches an onBeforeNavigate event.
void DispatchOnBeforeNavigate(NavigationController* controller,
                              ProvisionalLoadDetails* details,
                              uint64 request_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey, details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(details));
  dict->SetInteger(keys::kRequestIdKey, static_cast<int>(request_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnBeforeNavigate, json_args);
}

// Constructs and dispatches an onCommitted event.
void DispatchOnCommitted(NavigationController* controller,
                         ProvisionalLoadDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey, details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(details));
  dict->SetString(keys::kTransitionTypeKey,
                  PageTransition::CoreTransitionString(
                      details->transition_type()));
  ListValue* qualifiers = new ListValue();
  if (details->transition_type() & PageTransition::CLIENT_REDIRECT)
    qualifiers->Append(Value::CreateStringValue("client_redirect"));
  if (details->transition_type() & PageTransition::SERVER_REDIRECT)
    qualifiers->Append(Value::CreateStringValue("server_redirect"));
  if (details->transition_type() & PageTransition::FORWARD_BACK)
    qualifiers->Append(Value::CreateStringValue("forward_back"));
  dict->Set(keys::kTransitionQualifiersKey, qualifiers);
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnCommitted, json_args);
}

// Constructs and dispatches an onDOMContentLoaded event.
void DispatchOnDOMContentLoaded(NavigationController* controller,
                                const GURL& url,
                                bool is_main_frame,
                                int64 frame_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey,
      is_main_frame ? 0 : static_cast<int>(frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnDOMContentLoaded, json_args);
}

// Constructs and dispatches an onCompleted event.
void DispatchOnCompleted(NavigationController* controller,
                         const GURL& url,
                         bool is_main_frame,
                         int64 frame_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey,
      is_main_frame ? 0 : static_cast<int>(frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnCompleted, json_args);
}

}  // namespace


FrameNavigationState::FrameNavigationState()
    : allow_extension_scheme_(false) {
}

FrameNavigationState::~FrameNavigationState() {
}

bool FrameNavigationState::CanSendEvents(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end() ||
      frame_state->second.error_occurred) {
    return false;
  }
  const std::string& scheme = frame_state->second.url.scheme();
  for (unsigned i = 0; i < arraysize(kValidSchemes); ++i) {
    if (scheme == kValidSchemes[i])
      return true;
  }
  if (allow_extension_scheme_ && scheme == chrome::kExtensionScheme)
    return true;
  return false;
}

void FrameNavigationState::TrackFrame(int64 frame_id,
                                      const GURL& url,
                                      bool is_main_frame,
                                      bool is_error_page,
                                      const TabContents* tab_contents) {
  if (is_main_frame)
    RemoveTabContentsState(tab_contents);
  tab_contents_map_.insert(std::make_pair(tab_contents, frame_id));
  FrameState& frame_state = frame_state_map_[frame_id];
  frame_state.error_occurred = is_error_page;
  frame_state.url = url;
  frame_state.is_main_frame = is_main_frame;
}

GURL FrameNavigationState::GetUrl(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return GURL();
  }
  return frame_state->second.url;
}

bool FrameNavigationState::IsMainFrame(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return false;
  }
  return frame_state->second.is_main_frame;
}

void FrameNavigationState::ErrorOccurredInFrame(int64 frame_id) {
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
                   NotificationType::CREATING_NEW_WINDOW,
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
          *Details<int64>(details).ptr());
      break;
    case NotificationType::FRAME_DID_FINISH_LOAD:
      FrameDidFinishLoad(
          Source<NavigationController>(source).ptr(),
          *Details<int64>(details).ptr());
      break;
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      FailProvisionalLoadWithError(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NotificationType::CREATING_NEW_WINDOW:
      CreatingNewWindow(
          Source<TabContents>(source).ptr(),
          Details<const ViewHostMsg_CreateWindow_Params>(details).ptr());
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
                               details->is_error_page(),
                               controller->tab_contents());
  if (!navigation_state_.CanSendEvents(details->frame_id()))
    return;
  DispatchOnBeforeNavigate(controller, details, 0);
}

void ExtensionWebNavigationEventRouter::FrameProvisionalLoadCommitted(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  if (!navigation_state_.CanSendEvents(details->frame_id()))
    return;
  // On reference fragment navigations, only a new navigation state is
  // committed. We need to catch this case and generate a full sequence
  // of events.
  if (IsReferenceFragmentNavigation(details)) {
    NavigatedReferenceFragment(controller, details);
    return;
  }
  DispatchOnCommitted(controller, details);
}

void ExtensionWebNavigationEventRouter::FrameDomContentLoaded(
    NavigationController* controller,
    int64 frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnDOMContentLoaded(controller,
                             navigation_state_.GetUrl(frame_id),
                             navigation_state_.IsMainFrame(frame_id),
                             frame_id);
}

void ExtensionWebNavigationEventRouter::FrameDidFinishLoad(
    NavigationController* controller,
    int64 frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnCompleted(controller,
                      navigation_state_.GetUrl(frame_id),
                      navigation_state_.IsMainFrame(frame_id),
                      frame_id);
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
  dict->SetString(keys::kUrlKey, details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(details));
  dict->SetString(keys::kErrorKey,
                  std::string(net::ErrorToString(details->error_code())));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  navigation_state_.ErrorOccurredInFrame(details->frame_id());
  DispatchEvent(controller->profile(), keys::kOnErrorOccurred, json_args);
}

void ExtensionWebNavigationEventRouter::CreatingNewWindow(
    TabContents* tab_contents,
    const ViewHostMsg_CreateWindow_Params* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kSourceTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents));
  dict->SetString(keys::kSourceUrlKey, details->opener_url.spec());
  dict->SetString(keys::kUrlKey,
                  details->target_url.possibly_invalid_spec());
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(tab_contents->profile(), keys::kOnBeforeRetarget, json_args);
}

// See also NavigationController::IsURLInPageNavigation.
bool ExtensionWebNavigationEventRouter::IsReferenceFragmentNavigation(
    ProvisionalLoadDetails* details) {
  GURL existing_url = navigation_state_.GetUrl(details->frame_id());
  if (existing_url == details->url())
    return false;

  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
      details->url().ReplaceComponents(replacements);
}

void ExtensionWebNavigationEventRouter::NavigatedReferenceFragment(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  navigation_state_.TrackFrame(details->frame_id(),
                               details->url(),
                               details->main_frame(),
                               details->is_error_page(),
                               controller->tab_contents());

  DispatchOnBeforeNavigate(controller, details, 0);
  DispatchOnCommitted(controller, details);
  DispatchOnDOMContentLoaded(controller,
                             details->url(),
                             details->main_frame(),
                             details->frame_id());
  DispatchOnCompleted(controller,
                      details->url(),
                      details->main_frame(),
                      details->frame_id());
}
