// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/event_filtering_info.h"
#include "net/base/net_errors.h"

namespace extensions {

namespace keys = web_navigation_api_constants;

namespace web_navigation_api_helpers {

namespace {

// Returns |time| as milliseconds since the epoch.
double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

// Dispatches events to the extension message service.
void DispatchEvent(content::BrowserContext* browser_context,
                   const char* event_name,
                   scoped_ptr<ListValue> args,
                   const GURL& url) {
  EventFilteringInfo info;
  info.SetURL(url);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && extensions::ExtensionSystem::Get(profile)->event_router()) {
    scoped_ptr<Event> event(new Event(event_name, args.Pass()));
    event->restrict_to_profile = profile;
    event->filter_info = info;
    ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());
  }
}

}  // namespace

int GetFrameId(bool is_main_frame, int64 frame_id) {
  return is_main_frame ? 0 : static_cast<int>(frame_id);
}

// Constructs and dispatches an onBeforeNavigate event.
void DispatchOnBeforeNavigate(content::WebContents* web_contents,
                              int render_process_id,
                              int64 frame_id,
                              bool is_main_frame,
                              int64 parent_frame_id,
                              bool parent_is_main_frame,
                              const GURL& validated_url) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey, ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, validated_url.spec());
  dict->SetInteger(keys::kProcessIdKey, render_process_id);
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetInteger(keys::kParentFrameIdKey,
                   GetFrameId(parent_is_main_frame, parent_frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args->Append(dict);

  DispatchEvent(web_contents->GetBrowserContext(),
                keys::kOnBeforeNavigate,
                args.Pass(),
                validated_url);
}

// Constructs and dispatches an onCommitted or onReferenceFragmentUpdated
// event.
void DispatchOnCommitted(const char* event_name,
                         content::WebContents* web_contents,
                         int64 frame_id,
                         bool is_main_frame,
                         const GURL& url,
                         content::PageTransition transition_type) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey, ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kProcessIdKey,
                   web_contents->GetRenderViewHost()->GetProcess()->GetID());
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
  args->Append(dict);

  DispatchEvent(web_contents->GetBrowserContext(), event_name, args.Pass(),
                url);
}

// Constructs and dispatches an onDOMContentLoaded event.
void DispatchOnDOMContentLoaded(content::WebContents* web_contents,
                                const GURL& url,
                                bool is_main_frame,
                                int64 frame_id) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kProcessIdKey,
                   web_contents->GetRenderViewHost()->GetProcess()->GetID());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args->Append(dict);

  DispatchEvent(web_contents->GetBrowserContext(),
                keys::kOnDOMContentLoaded,
                args.Pass(),
                url);
}

// Constructs and dispatches an onCompleted event.
void DispatchOnCompleted(content::WebContents* web_contents,
                         const GURL& url,
                         bool is_main_frame,
                         int64 frame_id) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kProcessIdKey,
                   web_contents->GetRenderViewHost()->GetProcess()->GetID());
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args->Append(dict);

  DispatchEvent(web_contents->GetBrowserContext(), keys::kOnCompleted,
                args.Pass(), url);
}

// Constructs and dispatches an onCreatedNavigationTarget event.
void DispatchOnCreatedNavigationTarget(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    int64 source_frame_id,
    bool source_frame_is_main_frame,
    content::WebContents* target_web_contents,
    const GURL& target_url) {
  // Check that the tab is already inserted into a tab strip model. This code
  // path is exercised by ExtensionApiTest.WebNavigationRequestOpenTab.
  DCHECK(ExtensionTabUtil::GetTabById(
      ExtensionTabUtil::GetTabId(target_web_contents),
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext()),
      false, NULL, NULL, NULL, NULL));

  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kSourceTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetInteger(keys::kSourceProcessIdKey,
                   web_contents->GetRenderViewHost()->GetProcess()->GetID());
  dict->SetInteger(keys::kSourceFrameIdKey,
      GetFrameId(source_frame_is_main_frame, source_frame_id));
  dict->SetString(keys::kUrlKey, target_url.possibly_invalid_spec());
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(target_web_contents));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args->Append(dict);

  DispatchEvent(browser_context, keys::kOnCreatedNavigationTarget, args.Pass(),
                target_url);
}

// Constructs and dispatches an onErrorOccurred event.
void DispatchOnErrorOccurred(content::WebContents* web_contents,
                             int render_process_id,
                             const GURL& url,
                             int64 frame_id,
                             bool is_main_frame,
                             int error_code) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey, ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetInteger(keys::kProcessIdKey, render_process_id);
  dict->SetInteger(keys::kFrameIdKey, GetFrameId(is_main_frame, frame_id));
  dict->SetString(keys::kErrorKey, net::ErrorToString(error_code));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args->Append(dict);

  DispatchEvent(web_contents->GetBrowserContext(), keys::kOnErrorOccurred,
                args.Pass(), url);
}

// Constructs and dispatches an onTabReplaced event.
void DispatchOnTabReplaced(
    content::WebContents* old_web_contents,
    content::BrowserContext* browser_context,
    content::WebContents* new_web_contents) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kReplacedTabIdKey,
                   ExtensionTabUtil::GetTabId(old_web_contents));
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(new_web_contents));
  dict->SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(base::Time::Now()));
  args->Append(dict);

  DispatchEvent(browser_context, keys::kOnTabReplaced, args.Pass(), GURL());
}

}  // namespace web_navigation_api_helpers

}  // namespace extensions
