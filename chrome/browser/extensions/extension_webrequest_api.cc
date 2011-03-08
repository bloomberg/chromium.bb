// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_webrequest_api_constants;

namespace {

// List of all the webRequest events.
static const char *kWebRequestEvents[] = {
  keys::kOnBeforeRedirect,
  keys::kOnBeforeRequest,
  keys::kOnCompleted,
  keys::kOnErrorOccurred,
  keys::kOnHeadersReceived,
  keys::kOnRequestSent
};

// TODO(mpcomplete): should this be a set of flags?
static const char* kRequestFilterTypes[] = {
  "main_frame",
  "sub_frame",
  "stylesheet",
  "script",
  "image",
  "object",
  "other",
};

static bool IsWebRequestEvent(const std::string& event_name) {
  return std::find(kWebRequestEvents,
                   kWebRequestEvents + arraysize(kWebRequestEvents),
                   event_name) !=
         kWebRequestEvents + arraysize(kWebRequestEvents);
}

static bool IsValidRequestFilterType(const std::string& type) {
  return std::find(kRequestFilterTypes,
                   kRequestFilterTypes + arraysize(kRequestFilterTypes),
                   type) !=
         kRequestFilterTypes + arraysize(kRequestFilterTypes);
}

static void AddEventListenerOnIOThread(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    const ExtensionWebRequestEventRouter::RequestFilter& filter,
    int extra_info_spec) {
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      profile_id, extension_id, event_name, sub_event_name, filter,
      extra_info_spec);
}

static void EventHandledOnIOThread(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64 request_id,
    bool cancel) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id, extension_id, event_name, sub_event_name, request_id, cancel);
}

}  // namespace

// Internal representation of the webRequest.RequestFilter type, used to
// filter what network events an extension cares about.
struct ExtensionWebRequestEventRouter::RequestFilter {
  ExtensionExtent urls;
  std::vector<std::string> types;
  int tab_id;
  int window_id;

  bool InitFromValue(const DictionaryValue& value);
};

// Internal representation of the extraInfoSpec parameter on webRequest events,
// used to specify extra information to be included with network events.
struct ExtensionWebRequestEventRouter::ExtraInfoSpec {
  enum Flags {
    REQUEST_LINE = 1<<0,
    REQUEST_HEADERS = 1<<1,
    STATUS_LINE = 1<<2,
    RESPONSE_HEADERS = 1<<3,
    REDIRECT_REQUEST_LINE = 1<<4,
    REDIRECT_REQUEST_HEADERS = 1<<5,
    BLOCKING = 1<<5,
  };

  static bool InitFromValue(const ListValue& value, int* extra_info_spec);
};

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
struct ExtensionWebRequestEventRouter::EventListener {
  std::string extension_id;
  std::string sub_event_name;
  RequestFilter filter;
  int extra_info_spec;
  mutable std::set<uint64> blocked_requests;

  // Comparator to work with std::set.
  bool operator<(const EventListener& that) const {
    if (extension_id < that.extension_id)
      return true;
    if (extension_id == that.extension_id &&
        sub_event_name < that.sub_event_name)
      return true;
    return false;
  }
};

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct ExtensionWebRequestEventRouter::BlockedRequest {
  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking;

  // The callback to call when we get a response from all event handlers.
  net::CompletionCallback* callback;

  // Time the request was issued. Used for logging purposes.
  base::Time request_time;

  BlockedRequest() : num_handlers_blocking(0), callback(NULL) {}
};

bool ExtensionWebRequestEventRouter::RequestFilter::InitFromValue(
    const DictionaryValue& value) {
  for (DictionaryValue::key_iterator key = value.begin_keys();
       key != value.end_keys(); ++key) {
    if (*key == "urls") {
      ListValue* urls_value = NULL;
      if (!value.GetList("urls", &urls_value))
        return false;
      for (size_t i = 0; i < urls_value->GetSize(); ++i) {
        std::string url;
        URLPattern pattern(URLPattern::SCHEME_ALL);
        if (!urls_value->GetString(i, &url) ||
            pattern.Parse(url, URLPattern::PARSE_STRICT) !=
                URLPattern::PARSE_SUCCESS)
          return false;
        urls.AddPattern(pattern);
      }
    } else if (*key == "types") {
      ListValue* types_value = NULL;
      if (!value.GetList("urls", &types_value))
        return false;
      for (size_t i = 0; i < types_value->GetSize(); ++i) {
        std::string type;
        if (!types_value->GetString(i, &type) ||
            !IsValidRequestFilterType(type))
          return false;
        types.push_back(type);
      }
    } else if (*key == "tabId") {
      if (!value.GetInteger("tabId", &tab_id))
        return false;
    } else if (*key == "windowId") {
      if (!value.GetInteger("windowId", &window_id))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

// static
bool ExtensionWebRequestEventRouter::ExtraInfoSpec::InitFromValue(
    const ListValue& value, int* extra_info_spec) {
  *extra_info_spec = 0;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    std::string str;
    if (!value.GetString(i, &str))
      return false;

    // TODO(mpcomplete): not all of these are valid for every event.
    if (str == "requestLine")
      *extra_info_spec |= REQUEST_LINE;
    else if (str == "requestHeaders")
      *extra_info_spec |= REQUEST_HEADERS;
    else if (str == "statusLine")
      *extra_info_spec |= STATUS_LINE;
    else if (str == "responseHeaders")
      *extra_info_spec |= RESPONSE_HEADERS;
    else if (str == "redirectRequestLine")
      *extra_info_spec |= REDIRECT_REQUEST_LINE;
    else if (str == "redirectRequestHeaders")
      *extra_info_spec |= REDIRECT_REQUEST_HEADERS;
    else if (str == "blocking")
      *extra_info_spec |= BLOCKING;
    else
      return false;
  }
  return true;
}

// static
ExtensionWebRequestEventRouter* ExtensionWebRequestEventRouter::GetInstance() {
  return Singleton<ExtensionWebRequestEventRouter>::get();
}

ExtensionWebRequestEventRouter::ExtensionWebRequestEventRouter() {
}

ExtensionWebRequestEventRouter::~ExtensionWebRequestEventRouter() {
}

bool ExtensionWebRequestEventRouter::OnBeforeRequest(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request,
    net::CompletionCallback* callback) {
  // TODO(jochen): Figure out what to do with events from the system context.
  if (profile_id == Profile::kInvalidProfileId)
    return false;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnBeforeRequest, request->url());
  if (listeners.empty())
    return false;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetString(keys::kMethodKey, request->method());
  // TODO(mpcomplete): implement
  dict->SetInteger(keys::kTabIdKey, 0);
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kTypeKey, "main_frame");
  dict->SetInteger(keys::kTimeStampKey, 1);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  // TODO(mpcomplete): Consider consolidating common (extension_id,json_args)
  // pairs into a single message sent to a list of sub_event_names.
  int num_handlers_blocking = 0;
  for (std::vector<const EventListener*>::iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    event_router->DispatchEventToExtension(
        (*it)->extension_id, (*it)->sub_event_name, json_args,
        profile_id, true, GURL());
    if ((*it)->extra_info_spec & ExtraInfoSpec::BLOCKING) {
      (*it)->blocked_requests.insert(request->identifier());
      ++num_handlers_blocking;
    }
  }

  if (num_handlers_blocking > 0) {
    CHECK(blocked_requests_.find(request->identifier()) ==
          blocked_requests_.end());
    blocked_requests_[request->identifier()].num_handlers_blocking =
        num_handlers_blocking;
    blocked_requests_[request->identifier()].callback = callback;
    blocked_requests_[request->identifier()].request_time =
        request->request_time();

    return true;
  }

  return false;
}

void ExtensionWebRequestEventRouter::OnEventHandled(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64 request_id,
    bool cancel) {
  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  // The listener may have been removed (e.g. due to the process going away)
  // before we got here.
  std::set<EventListener>::iterator found =
      listeners_[profile_id][event_name].find(listener);
  if (found != listeners_[profile_id][event_name].end())
    found->blocked_requests.erase(request_id);

  DecrementBlockCount(request_id, cancel);
}

void ExtensionWebRequestEventRouter::AddEventListener(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    const RequestFilter& filter,
    int extra_info_spec) {
  if (!IsWebRequestEvent(event_name))
    return;

  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;
  listener.filter = filter;
  listener.extra_info_spec = extra_info_spec;

  CHECK_EQ(listeners_[profile_id][event_name].count(listener), 0u) <<
      "extension=" << extension_id << " event=" << event_name;
  listeners_[profile_id][event_name].insert(listener);
}

void ExtensionWebRequestEventRouter::RemoveEventListener(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& sub_event_name) {
  size_t slash_sep = sub_event_name.find('/');
  std::string event_name = sub_event_name.substr(0, slash_sep);

  if (!IsWebRequestEvent(event_name))
    return;

  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  CHECK_EQ(listeners_[profile_id][event_name].count(listener), 1u) <<
      "extension=" << extension_id << " event=" << event_name;

  // Unblock any request that this event listener may have been blocking.
  std::set<EventListener>::iterator found =
      listeners_[profile_id][event_name].find(listener);
  for (std::set<uint64>::iterator it = found->blocked_requests.begin();
       it != found->blocked_requests.end(); ++it) {
    DecrementBlockCount(*it, false);
  }

  listeners_[profile_id][event_name].erase(listener);
}

std::vector<const ExtensionWebRequestEventRouter::EventListener*>
ExtensionWebRequestEventRouter::GetMatchingListeners(
    ProfileId profile_id,
    const std::string& event_name,
    const GURL& url) {
  // TODO(mpcomplete): handle profile_id == invalid (should collect all
  // listeners).
  std::vector<const EventListener*> matching_listeners;
  std::set<EventListener>& listeners = listeners_[profile_id][event_name];
  for (std::set<EventListener>::iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    if (it->filter.urls.is_empty() || it->filter.urls.ContainsURL(url))
      matching_listeners.push_back(&(*it));
  }
  return matching_listeners;
}

void ExtensionWebRequestEventRouter::DecrementBlockCount(uint64 request_id,
                                                         bool cancel) {
  // It's possible that this request was already cancelled by a previous event
  // handler. If so, ignore this response.
  if (blocked_requests_.find(request_id) == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = blocked_requests_[request_id];
  int num_handlers_blocking = --blocked_request.num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  HISTOGRAM_TIMES("Extensions.NetworkDelay",
                   base::Time::Now() - blocked_request.request_time);

  if (num_handlers_blocking == 0 || cancel) {
    CHECK(blocked_request.callback);
    blocked_request.callback->Run(cancel ? net::ERR_EMPTY_RESPONSE : net::OK);
    blocked_requests_.erase(request_id);
  }
}

bool WebRequestAddEventListener::RunImpl() {
  // Argument 0 is the callback, which we don't use here.

  ExtensionWebRequestEventRouter::RequestFilter filter;
  if (HasOptionalArgument(1)) {
    DictionaryValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &value));
    EXTENSION_FUNCTION_VALIDATE(filter.InitFromValue(*value));
  }

  int extra_info_spec = 0;
  if (HasOptionalArgument(2)) {
    ListValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetList(2, &value));
    EXTENSION_FUNCTION_VALIDATE(
        ExtensionWebRequestEventRouter::ExtraInfoSpec::InitFromValue(
            *value, &extra_info_spec));
  }

  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(3, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(4, &sub_event_name));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          &AddEventListenerOnIOThread,
          profile()->GetRuntimeId(), extension_id(),
          event_name, sub_event_name, filter, extra_info_spec));

  return true;
}

bool WebRequestEventHandled::RunImpl() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &sub_event_name));

  std::string request_id_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &request_id_str));
  // TODO(mpcomplete): string-to-uint64?
  int64 request_id;
  EXTENSION_FUNCTION_VALIDATE(base::StringToInt64(request_id_str, &request_id));

  bool cancel = false;
  if (HasOptionalArgument(3)) {
    DictionaryValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(3, &value));

    if (value->HasKey("cancel"))
      EXTENSION_FUNCTION_VALIDATE(value->GetBoolean("cancel", &cancel));
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          &EventHandledOnIOThread,
          profile()->GetRuntimeId(), extension_id(),
          event_name, sub_event_name, request_id, cancel));

  return true;
}
