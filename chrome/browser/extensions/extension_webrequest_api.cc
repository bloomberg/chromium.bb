// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_io_event_router.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
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
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    const ExtensionWebRequestEventRouter::RequestFilter& filter,
    int extra_info_spec) {
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      extension_id, event_name, sub_event_name, filter, extra_info_spec);
}

static void RemoveEventListenerOnIOThread(
    const std::string& extension_id, const std::string& sub_event_name) {
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      extension_id, sub_event_name);
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
            pattern.Parse(url) != URLPattern::PARSE_SUCCESS)
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

// static
void ExtensionWebRequestEventRouter::RemoveEventListenerOnUIThread(
    const std::string& extension_id, const std::string& sub_event_name) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          &RemoveEventListenerOnIOThread,
          extension_id, sub_event_name));
}

void ExtensionWebRequestEventRouter::OnBeforeRequest(
    const ExtensionIOEventRouter* event_router,
    const GURL& url,
    const std::string& method) {
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(keys::kOnBeforeRequest, url);
  if (listeners.empty())
    return;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetString(keys::kMethodKey, method);
  // TODO(mpcomplete): implement
  dict->SetInteger(keys::kTabIdKey, 0);
  dict->SetInteger(keys::kRequestIdKey, 0);
  dict->SetString(keys::kTypeKey, "main_frame");
  dict->SetInteger(keys::kTimeStampKey, 1);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  for (std::vector<const EventListener*>::iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    event_router->DispatchEventToExtension(
        (*it)->extension_id, (*it)->sub_event_name, json_args);
  }
}

void ExtensionWebRequestEventRouter::AddEventListener(
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

  CHECK_EQ(listeners_[event_name].count(listener), 0u) <<
      "extension=" << extension_id << " event=" << event_name;
  listeners_[event_name].insert(listener);
}

void ExtensionWebRequestEventRouter::RemoveEventListener(
    const std::string& extension_id,
    const std::string& sub_event_name) {
  size_t slash_sep = sub_event_name.find('/');
  std::string event_name = sub_event_name.substr(0, slash_sep);

  if (!IsWebRequestEvent(event_name))
    return;

  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  CHECK_EQ(listeners_[event_name].count(listener), 1u) <<
      "extension=" << extension_id << " event=" << event_name;
  listeners_[event_name].erase(listener);
}

std::vector<const ExtensionWebRequestEventRouter::EventListener*>
ExtensionWebRequestEventRouter::GetMatchingListeners(
    const std::string& event_name, const GURL& url) {
  std::vector<const EventListener*> matching_listeners;
  std::set<EventListener>& listeners = listeners_[event_name];
  for (std::set<EventListener>::iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    if (it->filter.urls.is_empty() || it->filter.urls.ContainsURL(url))
      matching_listeners.push_back(&(*it));
  }
  return matching_listeners;
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
          extension_id(), event_name, sub_event_name, filter, extra_info_spec));

  return true;
}
