// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using content::BrowserContext;
using content::ResourceRedirectDetails;
using content::WebContents;

namespace extensions {

namespace keys = web_navigation_api_constants;

namespace {

typedef std::map<WebContents*, WebNavigationTabObserver*> TabObserverMap;
static base::LazyInstance<TabObserverMap> g_tab_observer =
    LAZY_INSTANCE_INITIALIZER;

// URL schemes for which we'll send events.
const char* kValidSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kJavaScriptScheme,
  chrome::kDataScheme,
  chrome::kFileSystemScheme,
};

// Returns the frame ID as it will be passed to the extension:
// 0 if the navigation happens in the main frame, or the frame ID
// modulo 32 bits otherwise.
// Keep this in sync with the GetFrameId() function in
// extension_webrequest_api.cc.
int GetFrameId(bool is_main_frame, int64 frame_id) {
  return is_main_frame ? 0 : static_cast<int>(frame_id);
}

// Returns |time| as milliseconds since the epoch.
double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

// Dispatches events to the extension message service.
void DispatchEvent(BrowserContext* browser_context,
                   const char* event_name,
                   const std::string& json_args) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL());
  }
}

// Constructs and dispatches an onBeforeNavigate event.
void DispatchOnBeforeNavigate(WebContents* web_contents,
                              int64 frame_id,
                              bool is_main_frame,
                              const GURL& validated_url) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey, ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, validated_url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(web_contents->GetBrowserContext(),
                keys::kOnBeforeNavigate,
                json_args);
}

// Constructs and dispatches an onCommitted or onReferenceFragmentUpdated
// event.
void DispatchOnCommitted(const char* event_name,
                         WebContents* web_contents,
                         int64 frame_id,
                         bool is_main_frame,
                         const GURL& url,
                         content::PageTransition transition_type) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey, ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetString(
      keys::kTransitionTypeKey,
      content::PageTransitionGetCoreTransitionString(transition_type));
  ListValue* qualifiers = new ListValue();
  if (transition_type & content::PAGE_TRANSITION_CLIENT_REDIRECT)
    qualifiers->Append(Value::CreateStringValue("client_redirect"));
  if (transition_type & content::PAGE_TRANSITION_SERVER_REDIRECT)
    qualifiers->Append(Value::CreateStringValue("server_redirect"));
  if (transition_type & content::PAGE_TRANSITION_FORWARD_BACK)
    qualifiers->Append(Value::CreateStringValue("forward_back"));
  if (transition_type & content::PAGE_TRANSITION_FROM_ADDRESS_BAR)
    qualifiers->Append(Value::CreateStringValue("from_address_bar"));
  dict->Set(keys::kTransitionQualifiersKey, qualifiers);
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(web_contents->GetBrowserContext(), event_name, json_args);
}

// Constructs and dispatches an onDOMContentLoaded event.
void DispatchOnDOMContentLoaded(WebContents* web_contents,
                                const GURL& url,
                                bool is_main_frame,
                                int64 frame_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(web_contents->GetBrowserContext(),
                keys::kOnDOMContentLoaded,
                json_args);
}

// Constructs and dispatches an onCompleted event.
void DispatchOnCompleted(WebContents* web_contents,
                         const GURL& url,
                         bool is_main_frame,
                         int64 frame_id) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(web_contents->GetBrowserContext(),
                keys::kOnCompleted, json_args);
}

// Constructs and dispatches an onCreatedNavigationTarget event.
void DispatchOnCreatedNavigationTarget(
    WebContents* web_contents,
    BrowserContext* browser_context,
    int64 source_frame_id,
    bool source_frame_is_main_frame,
    WebContents* target_web_contents,
    const GURL& target_url) {
  // Check that the tab is already inserted into a tab strip model. This code
  // path is exercised by ExtensionApiTest.WebNavigationRequestOpenTab.
  DCHECK(ExtensionTabUtil::GetTabById(
      ExtensionTabUtil::GetTabId(target_web_contents),
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext()),
      false, NULL, NULL, NULL, NULL));

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kSourceTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetInteger(keys::kSourceFrameIdKey,
      GetFrameId(source_frame_is_main_frame, source_frame_id));
  dict->SetString(keys::kUrlKey, target_url.possibly_invalid_spec());
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(target_web_contents));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(
      browser_context, keys::kOnCreatedNavigationTarget, json_args);
}

// Constructs and dispatches an onErrorOccurred event.
void DispatchOnErrorOccurred(WebContents* web_contents,
                             const GURL& url,
                             int64 frame_id,
                             bool is_main_frame,
                             int error_code) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey, ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetString(keys::kErrorKey, net::ErrorToString(error_code));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(web_contents->GetBrowserContext(),
                keys::kOnErrorOccurred,
                json_args);
}

}  // namespace


// FrameNavigationState -------------------------------------------------------

// static
bool FrameNavigationState::allow_extension_scheme_ = false;

FrameNavigationState::FrameNavigationState()
    : main_frame_id_(-1) {
}

FrameNavigationState::~FrameNavigationState() {}

bool FrameNavigationState::CanSendEvents(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end() ||
      frame_state->second.error_occurred) {
    return false;
  }
  return IsValidUrl(frame_state->second.url);
}

bool FrameNavigationState::IsValidUrl(const GURL& url) const {
  for (unsigned i = 0; i < arraysize(kValidSchemes); ++i) {
    if (url.scheme() == kValidSchemes[i])
      return true;
  }
  // Allow about:blank.
  if (url.spec() == chrome::kAboutBlankURL)
    return true;
  if (allow_extension_scheme_ && url.scheme() == chrome::kExtensionScheme)
    return true;
  return false;
}

void FrameNavigationState::TrackFrame(int64 frame_id,
                                      const GURL& url,
                                      bool is_main_frame,
                                      bool is_error_page) {
  if (is_main_frame) {
    frame_state_map_.clear();
    frame_ids_.clear();
  }
  FrameState& frame_state = frame_state_map_[frame_id];
  frame_state.error_occurred = is_error_page;
  frame_state.url = url;
  frame_state.is_main_frame = is_main_frame;
  frame_state.is_navigating = true;
  frame_state.is_committed = false;
  frame_state.is_server_redirected = false;
  if (is_main_frame) {
    main_frame_id_ = frame_id;
  }
  frame_ids_.insert(frame_id);
}

void FrameNavigationState::UpdateFrame(int64 frame_id, const GURL& url) {
  FrameIdToStateMap::iterator frame_state = frame_state_map_.find(frame_id);
  if (frame_state == frame_state_map_.end()) {
    NOTREACHED();
    return;
  }
  frame_state->second.url = url;
}

bool FrameNavigationState::IsValidFrame(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end());
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
  return main_frame_id_ != -1 && main_frame_id_ == frame_id;
}

int64 FrameNavigationState::GetMainFrameID() const {
  return main_frame_id_;
}

void FrameNavigationState::SetErrorOccurredInFrame(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].error_occurred = true;
}

bool FrameNavigationState::GetErrorOccurredInFrame(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state == frame_state_map_.end() ||
          frame_state->second.error_occurred);
}

void FrameNavigationState::SetNavigationCompleted(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_navigating = false;
}

bool FrameNavigationState::GetNavigationCompleted(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state == frame_state_map_.end() ||
          !frame_state->second.is_navigating);
}

void FrameNavigationState::SetNavigationCommitted(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_committed = true;
}

bool FrameNavigationState::GetNavigationCommitted(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_committed);
}

void FrameNavigationState::SetIsServerRedirected(int64 frame_id) {
  DCHECK(frame_state_map_.find(frame_id) != frame_state_map_.end());
  frame_state_map_[frame_id].is_server_redirected = true;
}

bool FrameNavigationState::GetIsServerRedirected(int64 frame_id) const {
  FrameIdToStateMap::const_iterator frame_state =
      frame_state_map_.find(frame_id);
  return (frame_state != frame_state_map_.end() &&
          frame_state->second.is_server_redirected);
}


// WebNavigtionEventRouter -------------------------------------------

WebNavigationEventRouter::PendingWebContents::PendingWebContents()
    : source_web_contents(NULL),
      source_frame_id(0),
      source_frame_is_main_frame(false),
      target_web_contents(NULL),
      target_url() {
}

WebNavigationEventRouter::PendingWebContents::PendingWebContents(
    WebContents* source_web_contents,
    int64 source_frame_id,
    bool source_frame_is_main_frame,
    WebContents* target_web_contents,
    const GURL& target_url)
    : source_web_contents(source_web_contents),
      source_frame_id(source_frame_id),
      source_frame_is_main_frame(source_frame_is_main_frame),
      target_web_contents(target_web_contents),
      target_url(target_url) {
}

WebNavigationEventRouter::PendingWebContents::~PendingWebContents() {}

WebNavigationEventRouter::WebNavigationEventRouter(Profile* profile)
    : profile_(profile) {}

WebNavigationEventRouter::~WebNavigationEventRouter() {}

void WebNavigationEventRouter::Init() {
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_RETARGETING,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_ADDED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::NotificationService::AllSources());
  }
}

void WebNavigationEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_RETARGETING: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->GetOriginalProfile() == profile_) {
        Retargeting(
            content::Details<const RetargetingDetails>(details).ptr());
      }
      break;
    }

    case chrome::NOTIFICATION_TAB_ADDED:
      TabAdded(content::Details<WebContents>(details).ptr());
      break;

    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED:
      TabDestroyed(content::Source<WebContents>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}

void WebNavigationEventRouter::Retargeting(const RetargetingDetails* details) {
  if (details->source_frame_id == 0)
    return;
  WebNavigationTabObserver* tab_observer =
      WebNavigationTabObserver::Get(details->source_web_contents);
  if (!tab_observer) {
    // If you hit this DCHECK(), please add reproduction steps to
    // http://crbug.com/109464.
    DCHECK(details->source_web_contents->GetViewType() !=
           chrome::VIEW_TYPE_TAB_CONTENTS);
    return;
  }
  const FrameNavigationState& frame_navigation_state =
      tab_observer->frame_navigation_state();

  if (!frame_navigation_state.CanSendEvents(details->source_frame_id))
    return;

  // If the WebContents was created as a response to an IPC from a renderer
  // (and therefore doesn't yet have a wrapper), or if it isn't yet inserted
  // into a tab strip, we need to delay the extension event until the
  // WebContents is fully initialized.
  if ((TabContentsWrapper::GetCurrentWrapperForContents(
       details->target_web_contents) == NULL) ||
      details->not_yet_in_tabstrip) {
    pending_web_contents_[details->target_web_contents] =
        PendingWebContents(
            details->source_web_contents,
            details->source_frame_id,
            frame_navigation_state.IsMainFrame(details->source_frame_id),
            details->target_web_contents,
            details->target_url);
  } else {
    DispatchOnCreatedNavigationTarget(
        details->source_web_contents,
        details->target_web_contents->GetBrowserContext(),
        details->source_frame_id,
        frame_navigation_state.IsMainFrame(details->source_frame_id),
        details->target_web_contents,
        details->target_url);
  }
}

void WebNavigationEventRouter::TabAdded(WebContents* tab) {
  std::map<WebContents*, PendingWebContents>::iterator iter =
      pending_web_contents_.find(tab);
  if (iter == pending_web_contents_.end())
    return;

  DispatchOnCreatedNavigationTarget(
      iter->second.source_web_contents,
      iter->second.target_web_contents->GetBrowserContext(),
      iter->second.source_frame_id,
      iter->second.source_frame_is_main_frame,
      iter->second.target_web_contents,
      iter->second.target_url);
  pending_web_contents_.erase(iter);
}

void WebNavigationEventRouter::TabDestroyed(WebContents* tab) {
  pending_web_contents_.erase(tab);
  for (std::map<WebContents*, PendingWebContents>::iterator i =
           pending_web_contents_.begin(); i != pending_web_contents_.end(); ) {
    if (i->second.source_web_contents == tab)
      pending_web_contents_.erase(i++);
    else
      ++i;
  }
}

// WebNavigationTabObserver ------------------------------------------

WebNavigationTabObserver::WebNavigationTabObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  g_tab_observer.Get().insert(TabObserverMap::value_type(web_contents, this));
  registrar_.Add(this,
                 content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                 content::Source<WebContents>(web_contents));
}

WebNavigationTabObserver::~WebNavigationTabObserver() {}

// static
WebNavigationTabObserver* WebNavigationTabObserver::Get(
    WebContents* web_contents) {
  TabObserverMap::iterator i = g_tab_observer.Get().find(web_contents);
  return i == g_tab_observer.Get().end() ? NULL : i->second;
}

void WebNavigationTabObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      ResourceRedirectDetails* resource_redirect_details =
          content::Details<ResourceRedirectDetails>(details).ptr();
      ResourceType::Type resource_type =
          resource_redirect_details->resource_type;
      if (resource_type == ResourceType::MAIN_FRAME ||
          resource_type == ResourceType::SUB_FRAME) {
        int64 frame_id = resource_redirect_details->frame_id;
        if (!navigation_state_.CanSendEvents(frame_id))
          return;
        navigation_state_.SetIsServerRedirected(frame_id);
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

void WebNavigationTabObserver::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    content::RenderViewHost* render_view_host) {
  // Ignore navigations of sub frames, if the main frame isn't committed yet.
  // This might happen if a sub frame triggers a navigation for both the main
  // frame and itself. Since the sub frame is about to be deleted, and there's
  // no way for an extension to tell that these navigations belong to an old
  // frame, we just suppress the events here.
  int64 main_frame_id = navigation_state_.GetMainFrameID();
  if (!is_main_frame &&
      !navigation_state_.GetNavigationCommitted(main_frame_id)) {
    return;
  }

  navigation_state_.TrackFrame(frame_id,
                               validated_url,
                               is_main_frame,
                               is_error_page);
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnBeforeNavigate(
      web_contents(), frame_id, is_main_frame, validated_url);
}

void WebNavigationTabObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;

  bool is_reference_fragment_navigation =
      IsReferenceFragmentNavigation(frame_id, url);

  // Update the URL as it might have changed.
  navigation_state_.UpdateFrame(frame_id, url);
  navigation_state_.SetNavigationCommitted(frame_id);

  if (is_reference_fragment_navigation) {
    DispatchOnCommitted(
        keys::kOnReferenceFragmentUpdated,
        web_contents(),
        frame_id,
        is_main_frame,
        url,
        transition_type);
    navigation_state_.SetNavigationCompleted(frame_id);
  } else {
    if (navigation_state_.GetIsServerRedirected(frame_id)) {
      transition_type = static_cast<content::PageTransition>(
          transition_type | content::PAGE_TRANSITION_SERVER_REDIRECT);
    }
    DispatchOnCommitted(
        keys::kOnCommitted,
        web_contents(),
        frame_id,
        is_main_frame,
        url,
        transition_type);
  }
}

void WebNavigationTabObserver::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  navigation_state_.SetErrorOccurredInFrame(frame_id);
  DispatchOnErrorOccurred(
      web_contents(), validated_url, frame_id, is_main_frame, error_code);
}

void WebNavigationTabObserver::DocumentLoadedInFrame(
    int64 frame_id) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  DispatchOnDOMContentLoaded(web_contents(),
                             navigation_state_.GetUrl(frame_id),
                             navigation_state_.IsMainFrame(frame_id),
                             frame_id);
}

void WebNavigationTabObserver::DidFinishLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame) {
  if (!navigation_state_.CanSendEvents(frame_id))
    return;
  navigation_state_.SetNavigationCompleted(frame_id);
  DCHECK_EQ(navigation_state_.GetUrl(frame_id), validated_url);
  DCHECK_EQ(navigation_state_.IsMainFrame(frame_id), is_main_frame);
  DispatchOnCompleted(web_contents(),
                      validated_url,
                      is_main_frame,
                      frame_id);
}

void WebNavigationTabObserver::DidOpenRequestedURL(
    WebContents* new_contents,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    content::PageTransition transition,
    int64 source_frame_id) {
  if (!navigation_state_.CanSendEvents(source_frame_id))
    return;

  // We only send the onCreatedNavigationTarget if we end up creating a new
  // window.
  if (disposition != SINGLETON_TAB &&
      disposition != NEW_FOREGROUND_TAB &&
      disposition != NEW_BACKGROUND_TAB &&
      disposition != NEW_POPUP &&
      disposition != NEW_WINDOW &&
      disposition != OFF_THE_RECORD)
    return;

  DispatchOnCreatedNavigationTarget(
      web_contents(),
      new_contents->GetBrowserContext(),
      source_frame_id,
      navigation_state_.IsMainFrame(source_frame_id),
      new_contents,
      url);
}

void WebNavigationTabObserver::WebContentsDestroyed(WebContents* tab) {
  g_tab_observer.Get().erase(tab);
  for (FrameNavigationState::const_iterator frame = navigation_state_.begin();
       frame != navigation_state_.end(); ++frame) {
    if (!navigation_state_.GetNavigationCompleted(*frame) &&
        navigation_state_.CanSendEvents(*frame)) {
      DispatchOnErrorOccurred(
        tab,
        navigation_state_.GetUrl(*frame),
        *frame,
        navigation_state_.IsMainFrame(*frame),
        net::ERR_ABORTED);
    }
  }
}

// See also NavigationController::IsURLInPageNavigation.
bool WebNavigationTabObserver::IsReferenceFragmentNavigation(
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

bool GetFrameFunction::RunImpl() {
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  int tab_id;
  int frame_id;
  EXTENSION_FUNCTION_VALIDATE(details->GetInteger(keys::kTabIdKey, &tab_id));
  EXTENSION_FUNCTION_VALIDATE(
      details->GetInteger(keys::kFrameIdKey, &frame_id));

  result_.reset(Value::CreateNullValue());

  TabContentsWrapper* wrapper;
  if (!ExtensionTabUtil::GetTabById(
        tab_id, profile(), include_incognito(), NULL, NULL, &wrapper, NULL) ||
      !wrapper) {
    return true;
  }

  WebContents* web_contents = wrapper->web_contents();
  WebNavigationTabObserver* observer =
      WebNavigationTabObserver::Get(web_contents);
  DCHECK(observer);

  const FrameNavigationState& frame_navigation_state =
      observer->frame_navigation_state();

  if (frame_id == 0)
    frame_id = frame_navigation_state.GetMainFrameID();
  if (!frame_navigation_state.IsValidFrame(frame_id))
    return true;

  GURL frame_url = frame_navigation_state.GetUrl(frame_id);
  if (!frame_navigation_state.IsValidUrl(frame_url))
    return true;

  DictionaryValue* resultDict = new DictionaryValue();
  resultDict->SetString(keys::kUrlKey, frame_url.spec());
  resultDict->SetBoolean(
      keys::kErrorOccurredKey,
      frame_navigation_state.GetErrorOccurredInFrame(frame_id));
  result_.reset(resultDict);
  return true;
}

bool GetAllFramesFunction::RunImpl() {
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(details->GetInteger(keys::kTabIdKey, &tab_id));

  result_.reset(Value::CreateNullValue());

  TabContentsWrapper* wrapper;
  if (!ExtensionTabUtil::GetTabById(
        tab_id, profile(), include_incognito(), NULL, NULL, &wrapper, NULL) ||
      !wrapper) {
    return true;
  }

  WebContents* web_contents = wrapper->web_contents();
  WebNavigationTabObserver* observer =
      WebNavigationTabObserver::Get(web_contents);
  DCHECK(observer);

  const FrameNavigationState& navigation_state =
      observer->frame_navigation_state();

  ListValue* resultList = new ListValue();
  for (FrameNavigationState::const_iterator frame = navigation_state.begin();
       frame != navigation_state.end(); ++frame) {
    GURL frame_url = navigation_state.GetUrl(*frame);
    if (!navigation_state.IsValidUrl(frame_url))
      continue;
    DictionaryValue* frameDict = new DictionaryValue();
    frameDict->SetString(keys::kUrlKey, frame_url.spec());
    frameDict->SetInteger(
        keys::kFrameIdKey,
        GetFrameId(navigation_state.IsMainFrame(*frame), *frame));
    frameDict->SetBoolean(
        keys::kErrorOccurredKey,
        navigation_state.GetErrorOccurredInFrame(*frame));
    resultList->Append(frameDict);
  }
  result_.reset(resultList);
  return true;
}

}  // namespace extensions
