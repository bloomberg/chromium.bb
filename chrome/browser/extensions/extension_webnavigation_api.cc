// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/extension_webnavigation_api.h"

#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
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
int GetFrameId(bool is_main_frame, int64 frame_id) {
  return is_main_frame ? 0 : static_cast<int>(frame_id);
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
void DispatchOnBeforeNavigate(TabContents* tab_contents,
                              int64 frame_id,
                              bool is_main_frame,
                              const GURL& validated_url,
                              uint64 request_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents));
  dict->SetString(keys::kUrlKey, validated_url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(tab_contents->profile(), keys::kOnBeforeNavigate, json_args);
}

// Constructs and dispatches an onCommitted event.
void DispatchOnCommitted(TabContents* tab_contents,
                         int64 frame_id,
                         bool is_main_frame,
                         const GURL& url,
                         PageTransition::Type transition_type) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetString(keys::kTransitionTypeKey,
                  PageTransition::CoreTransitionString(transition_type));
  ListValue* qualifiers = new ListValue();
  if (transition_type & PageTransition::CLIENT_REDIRECT)
    qualifiers->Append(Value::CreateStringValue("client_redirect"));
  if (transition_type & PageTransition::SERVER_REDIRECT)
    qualifiers->Append(Value::CreateStringValue("server_redirect"));
  if (transition_type & PageTransition::FORWARD_BACK)
    qualifiers->Append(Value::CreateStringValue("forward_back"));
  dict->Set(keys::kTransitionQualifiersKey, qualifiers);
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(tab_contents->profile(), keys::kOnCommitted, json_args);
}

// Constructs and dispatches an onDOMContentLoaded event.
void DispatchOnDOMContentLoaded(TabContents* tab_contents,
                                const GURL& url,
                                bool is_main_frame,
                                int64 frame_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey,
      is_main_frame ? 0 : static_cast<int>(frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(tab_contents->profile(), keys::kOnDOMContentLoaded, json_args);
}

// Constructs and dispatches an onCompleted event.
void DispatchOnCompleted(TabContents* tab_contents,
                         const GURL& url,
                         bool is_main_frame,
                         int64 frame_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey,
      is_main_frame ? 0 : static_cast<int>(frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(tab_contents->profile(), keys::kOnCompleted, json_args);
}

// Constructs and dispatches an onBeforeRetarget event.
void DispatchOnBeforeRetarget(TabContents* tab_contents,
                              Profile* profile,
                              const GURL& opener_url,
                              const GURL& target_url) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kSourceTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents));
  dict->SetString(keys::kSourceUrlKey, opener_url.spec());
  dict->SetString(keys::kUrlKey, target_url.possibly_invalid_spec());
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(profile, keys::kOnBeforeRetarget, json_args);
}

}  // namespace


// FrameNavigationState -------------------------------------------------------

// static
bool FrameNavigationState::allow_extension_scheme_ = false;

FrameNavigationState::FrameNavigationState() {}

FrameNavigationState::~FrameNavigationState() {}

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


// ExtensionWebNavigtionEventRouter -------------------------------------------

ExtensionWebNavigationEventRouter::ExtensionWebNavigationEventRouter() {}

ExtensionWebNavigationEventRouter::~ExtensionWebNavigationEventRouter() {}

// static
ExtensionWebNavigationEventRouter*
ExtensionWebNavigationEventRouter::GetInstance() {
  return Singleton<ExtensionWebNavigationEventRouter>::get();
}

void ExtensionWebNavigationEventRouter::Init() {
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::CREATING_NEW_WINDOW,
                   NotificationService::AllSources());
  }
}

void ExtensionWebNavigationEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::CREATING_NEW_WINDOW:
      CreatingNewWindow(
          Source<TabContents>(source).ptr(),
          Details<const ViewHostMsg_CreateWindow_Params>(details).ptr());
      break;

    default:
      NOTREACHED();
  }
}

void ExtensionWebNavigationEventRouter::CreatingNewWindow(
    TabContents* tab_contents,
    const ViewHostMsg_CreateWindow_Params* details) {
  DispatchOnBeforeRetarget(tab_contents,
                           tab_contents->profile(),
                           details->opener_url,
                           details->target_url);
}


// ExtensionWebNavigationTabObserver ------------------------------------------

ExtensionWebNavigationTabObserver::ExtensionWebNavigationTabObserver(
    TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {}

ExtensionWebNavigationTabObserver::~ExtensionWebNavigationTabObserver() {}

void ExtensionWebNavigationTabObserver::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page) {
  navigation_state_.TrackFrame(frame_id,
                               validated_url,
                               is_main_frame,
                               is_error_page,
                               tab_contents());
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnBeforeNavigate(
      tab_contents(), frame_id, is_main_frame, validated_url, 0);
}

void ExtensionWebNavigationTabObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition::Type transition_type) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  // On reference fragment navigations, only a new navigation state is
  // committed. We need to catch this case and generate a full sequence
  // of events.
  if (IsReferenceFragmentNavigation(frame_id, url)) {
    NavigatedReferenceFragment(frame_id, is_main_frame, url, transition_type);
    return;
  }
  DispatchOnCommitted(
      tab_contents(), frame_id, is_main_frame, url, transition_type);
}

void ExtensionWebNavigationTabObserver::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(tab_contents()));
  dict->SetString(keys::kUrlKey, validated_url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetString(keys::kErrorKey,
                  std::string(net::ErrorToString(error_code)));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  navigation_state_.ErrorOccurredInFrame(frame_id);
  DispatchEvent(tab_contents()->profile(), keys::kOnErrorOccurred, json_args);
}

void ExtensionWebNavigationTabObserver::DocumentLoadedInFrame(
    int64 frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnDOMContentLoaded(tab_contents(),
                             navigation_state_.GetUrl(frame_id),
                             navigation_state_.IsMainFrame(frame_id),
                             frame_id);
}

void ExtensionWebNavigationTabObserver::DidFinishLoad(
    int64 frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnCompleted(tab_contents(),
                      navigation_state_.GetUrl(frame_id),
                      navigation_state_.IsMainFrame(frame_id),
                      frame_id);
}

void ExtensionWebNavigationTabObserver::TabContentsDestroyed(
    TabContents* tab) {
  navigation_state_.RemoveTabContentsState(tab);
}

void ExtensionWebNavigationTabObserver::DidOpenURL(
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  if (disposition != NEW_FOREGROUND_TAB &&
      disposition != NEW_BACKGROUND_TAB &&
      disposition != NEW_WINDOW &&
      disposition != OFF_THE_RECORD) {
    return;
  }
  Profile* profile = tab_contents()->profile();
  if (disposition == OFF_THE_RECORD) {
    if (!profile->HasOffTheRecordProfile()) {
      NOTREACHED();
      return;
    }
    profile = profile->GetOffTheRecordProfile();
  }
  DispatchOnBeforeRetarget(tab_contents(),
                           profile,
                           tab_contents()->GetURL(),
                           url);
}

// See also NavigationController::IsURLInPageNavigation.
bool ExtensionWebNavigationTabObserver::IsReferenceFragmentNavigation(
    int64 frame_id,
    const GURL& url) {
  GURL existing_url = navigation_state_.GetUrl(frame_id);
  if (existing_url == url)
    return false;

  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
      url.ReplaceComponents(replacements);
}

void ExtensionWebNavigationTabObserver::NavigatedReferenceFragment(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition::Type transition_type) {
  navigation_state_.TrackFrame(frame_id,
                               url,
                               is_main_frame,
                               false,
                               tab_contents());

  DispatchOnBeforeNavigate(tab_contents(),
                           frame_id,
                           is_main_frame,
                           url,
                           0);
  DispatchOnCommitted(tab_contents(),
                      frame_id,
                      is_main_frame,
                      url,
                      transition_type);
  DispatchOnDOMContentLoaded(tab_contents(),
                             url,
                             is_main_frame,
                             frame_id);
  DispatchOnCompleted(tab_contents(),
                      url,
                      is_main_frame,
                      frame_id);
}
