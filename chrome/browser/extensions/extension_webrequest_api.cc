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
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_id_map.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_webrequest_api_constants;

namespace {

// List of all the webRequest events.
static const char* const kWebRequestEvents[] = {
  keys::kOnBeforeRedirect,
  keys::kOnBeforeRequest,
  keys::kOnBeforeSendHeaders,
  keys::kOnCompleted,
  keys::kOnErrorOccurred,
  keys::kOnRequestSent,
  keys::kOnResponseStarted
};

static const char* kResourceTypeStrings[] = {
  "main_frame",
  "sub_frame",
  "stylesheet",
  "script",
  "image",
  "object",
  "other",
};

static ResourceType::Type kResourceTypeValues[] = {
  ResourceType::MAIN_FRAME,
  ResourceType::SUB_FRAME,
  ResourceType::STYLESHEET,
  ResourceType::SCRIPT,
  ResourceType::IMAGE,
  ResourceType::OBJECT,
  ResourceType::LAST_TYPE,  // represents "other"
};

COMPILE_ASSERT(
    arraysize(kResourceTypeStrings) == arraysize(kResourceTypeValues),
    keep_resource_types_in_sync);

#define ARRAYEND(array) (array + arraysize(array))

bool IsWebRequestEvent(const std::string& event_name) {
  return std::find(kWebRequestEvents, ARRAYEND(kWebRequestEvents),
                   event_name) != ARRAYEND(kWebRequestEvents);
}

const char* ResourceTypeToString(ResourceType::Type type) {
  ResourceType::Type* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues), type);
  if (iter == ARRAYEND(kResourceTypeValues))
    return "other";

  return kResourceTypeStrings[iter - kResourceTypeValues];
}

bool ParseResourceType(const std::string& type_str,
                              ResourceType::Type* type) {
  const char** iter =
      std::find(kResourceTypeStrings, ARRAYEND(kResourceTypeStrings), type_str);
  if (iter == ARRAYEND(kResourceTypeStrings))
    return false;
  *type = kResourceTypeValues[iter - kResourceTypeStrings];
  return true;
}

void ExtractRequestInfo(net::URLRequest* request,
                               int* tab_id,
                               int* window_id,
                               ResourceType::Type* resource_type) {
  if (!request->GetUserData(NULL))
    return;

  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  ExtensionTabIdMap::GetInstance()->GetTabAndWindowId(
      info->child_id(), info->route_id(), tab_id, window_id);

  // Restrict the resource type to the values we care about.
  ResourceType::Type* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues),
                info->resource_type());
  *resource_type = (iter != ARRAYEND(kResourceTypeValues)) ?
      *iter : ResourceType::LAST_TYPE;
}

void AddEventListenerOnIOThread(
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

void EventHandledOnIOThread(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64 request_id,
    ExtensionWebRequestEventRouter::EventResponse* response) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id, extension_id, event_name, sub_event_name, request_id,
      response);
}

// Creates a list of HttpHeaders (see extension_api.json). If |headers| is
// NULL, the list is empty. Ownership is passed to the caller.
ListValue* GetResponseHeadersList(const net::HttpResponseHeaders* headers) {
  ListValue* headers_value = new ListValue();
  if (headers) {
    void* iter = NULL;
    std::string name;
    std::string value;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      DictionaryValue* header = new DictionaryValue();
      header->SetString(keys::kHeaderNameKey, name);
      header->SetString(keys::kHeaderValueKey, value);
      headers_value->Append(header);
    }
  }
  return headers_value;
}

ListValue* GetRequestHeadersList(const net::HttpRequestHeaders* headers) {
  ListValue* headers_value = new ListValue();
  if (headers) {
    for (net::HttpRequestHeaders::Iterator it(*headers); it.GetNext(); ) {
      DictionaryValue* header = new DictionaryValue();
      header->SetString(keys::kHeaderNameKey, it.name());
      header->SetString(keys::kHeaderValueKey, it.value());
      headers_value->Append(header);
    }
  }
  return headers_value;
}

// Creates a StringValue with the status line of |headers|. If |headers| is
// NULL, an empty string is returned.  Ownership is passed to the caller.
StringValue* GetStatusLine(net::HttpResponseHeaders* headers) {
  return new StringValue(headers ? headers->GetStatusLine() : "");
}

}  // namespace

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

  EventListener() : extra_info_spec(0) {}
};

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct ExtensionWebRequestEventRouter::BlockedRequest {
  // The event that we're currently blocked on.
  EventTypes event;

  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking;

  // The callback to call when we get a response from all event handlers.
  net::CompletionCallback* callback;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest.
  GURL* new_url;

  // The request headers that will be issued along with this request. Only valid
  // for OnBeforeSendHeaders.
  net::HttpRequestHeaders* request_headers;

  // Time the request was paused. Used for logging purposes.
  base::Time blocking_time;

  // If non-NULL, this is the response we have chosen so far. Once all responses
  // are received, this is the one that will be used to determine how to proceed
  // with the request.
  linked_ptr<ExtensionWebRequestEventRouter::EventResponse> chosen_response;

  BlockedRequest()
      : event(kInvalidEvent),
        num_handlers_blocking(0),
        callback(NULL),
        new_url(NULL),
        request_headers(NULL) {}
};

bool ExtensionWebRequestEventRouter::RequestFilter::InitFromValue(
    const DictionaryValue& value, std::string* error) {
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
                URLPattern::PARSE_SUCCESS) {
          *error = ExtensionErrorUtils::FormatErrorMessage(
              keys::kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (*key == "types") {
      ListValue* types_value = NULL;
      if (!value.GetList("types", &types_value))
        return false;
      for (size_t i = 0; i < types_value->GetSize(); ++i) {
        std::string type_str;
        ResourceType::Type type;
        if (!types_value->GetString(i, &type_str) ||
            !ParseResourceType(type_str, &type))
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

    if (str == "requestLine")
      *extra_info_spec |= REQUEST_LINE;
    else if (str == "requestHeaders")
      *extra_info_spec |= REQUEST_HEADERS;
    else if (str == "statusLine")
      *extra_info_spec |= STATUS_LINE;
    else if (str == "responseHeaders")
      *extra_info_spec |= RESPONSE_HEADERS;
    else if (str == "blocking")
      *extra_info_spec |= BLOCKING;
    else
      return false;
  }
  return true;
}


ExtensionWebRequestEventRouter::EventResponse::EventResponse(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

ExtensionWebRequestEventRouter::EventResponse::~EventResponse() {
}


ExtensionWebRequestEventRouter::RequestFilter::RequestFilter()
    : tab_id(-1), window_id(-1) {
}

ExtensionWebRequestEventRouter::RequestFilter::~RequestFilter() {
}

//
// ExtensionWebRequestEventRouter
//

// static
ExtensionWebRequestEventRouter* ExtensionWebRequestEventRouter::GetInstance() {
  return Singleton<ExtensionWebRequestEventRouter>::get();
}

ExtensionWebRequestEventRouter::ExtensionWebRequestEventRouter() {
}

ExtensionWebRequestEventRouter::~ExtensionWebRequestEventRouter() {
}

int ExtensionWebRequestEventRouter::OnBeforeRequest(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request,
    net::CompletionCallback* callback,
    GURL* new_url) {
  // TODO(jochen): Figure out what to do with events from the system context.
  if (profile_id == Profile::kInvalidProfileId)
    return net::OK;

  // If this is an HTTP request, keep track of it. HTTP-specific events only
  // have the request ID, so we'll need to look up the URLRequest from that.
  // We need to do this even if no extension subscribes to OnBeforeRequest to
  // guarantee that |http_requests_| is populated if an extension subscribes
  // to OnBeforeSendHeaders or OnRequestSent.
  if (request->url().SchemeIs(chrome::kHttpScheme) ||
      request->url().SchemeIs(chrome::kHttpsScheme)) {
    http_requests_[request->identifier()] = request;
  }

  int tab_id = -1;
  int window_id = -1;
  ResourceType::Type resource_type = ResourceType::LAST_TYPE;
  ExtractRequestInfo(request, &tab_id, &window_id, &resource_type);

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnBeforeRequest, request->url(),
                           tab_id, window_id, resource_type, &extra_info_spec);
  if (listeners.empty())
    return net::OK;

  if (GetAndSetSignaled(request->identifier(), kOnBeforeRequest))
    return net::OK;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetString(keys::kMethodKey, request->method());
  dict->SetInteger(keys::kTabIdKey, tab_id);
  dict->SetString(keys::kTypeKey, ResourceTypeToString(resource_type));
  dict->SetDouble(keys::kTimeStampKey, base::Time::Now().ToDoubleT() * 1000);
  args.Append(dict);

  if (DispatchEvent(profile_id, event_router, request, listeners, args)) {
    blocked_requests_[request->identifier()].event = kOnBeforeRequest;
    blocked_requests_[request->identifier()].callback = callback;
    blocked_requests_[request->identifier()].new_url = new_url;
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int ExtensionWebRequestEventRouter::OnBeforeSendHeaders(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    uint64 request_id,
    net::CompletionCallback* callback,
    net::HttpRequestHeaders* headers) {
  // TODO(jochen): Figure out what to do with events from the system context.
  if (profile_id == Profile::kInvalidProfileId)
    return net::OK;

  HttpRequestMap::iterator iter = http_requests_.find(request_id);
  if (iter == http_requests_.end())
    return net::OK;

  net::URLRequest* request = iter->second;

  if (GetAndSetSignaled(request->identifier(), kOnBeforeSendHeaders))
    return net::OK;

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnBeforeSendHeaders, request,
                           &extra_info_spec);
  if (listeners.empty())
    return net::OK;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetDouble(keys::kTimeStampKey, base::Time::Now().ToDoubleT() * 1000);

  if (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS)
    dict->Set(keys::kRequestHeadersKey, GetRequestHeadersList(headers));
  // TODO(battre): implement request line.

  args.Append(dict);

  if (DispatchEvent(profile_id, event_router, request, listeners, args)) {
    blocked_requests_[request->identifier()].event = kOnBeforeSendHeaders;
    blocked_requests_[request->identifier()].callback = callback;
    blocked_requests_[request->identifier()].request_headers = headers;
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

void ExtensionWebRequestEventRouter::OnRequestSent(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    uint64 request_id,
    const net::HostPortPair& socket_address,
    const net::HttpRequestHeaders& headers) {
  if (profile_id == Profile::kInvalidProfileId)
    return;
  base::Time time(base::Time::Now());

  HttpRequestMap::iterator iter = http_requests_.find(request_id);
  if (iter == http_requests_.end())
    return;

  net::URLRequest* request = iter->second;

  if (GetAndSetSignaled(request->identifier(), kOnRequestSent))
    return;

  ClearSignaled(request->identifier(), kOnBeforeRedirect);

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnRequestSent, request,
                           &extra_info_spec);
  if (listeners.empty())
    return;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetString(keys::kIpKey, socket_address.host());
  dict->SetDouble(keys::kTimeStampKey, time.ToDoubleT() * 1000);
  if (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS)
    dict->Set(keys::kRequestHeadersKey, GetRequestHeadersList(&headers));
  // TODO(battre): support "request line".
  args.Append(dict);

  DispatchEvent(profile_id, event_router, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnBeforeRedirect(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request,
    const GURL& new_location) {
  if (profile_id == Profile::kInvalidProfileId)
    return;
  base::Time time(base::Time::Now());

  if (GetAndSetSignaled(request->identifier(), kOnBeforeRedirect))
    return;

  ClearSignaled(request->identifier(), kOnBeforeRequest);
  ClearSignaled(request->identifier(), kOnBeforeSendHeaders);
  ClearSignaled(request->identifier(), kOnRequestSent);

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnBeforeRedirect, request,
                           &extra_info_spec);
  if (listeners.empty())
    return;

  int http_status_code = request->GetResponseCode();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetString(keys::kRedirectUrlKey, new_location.spec());
  dict->SetInteger(keys::kStatusCodeKey, http_status_code);
  dict->SetDouble(keys::kTimeStampKey, time.ToDoubleT() * 1000);
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  if (extra_info_spec & ExtraInfoSpec::STATUS_LINE)
    dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  args.Append(dict);

  DispatchEvent(profile_id, event_router, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnResponseStarted(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request) {
  if (profile_id == Profile::kInvalidProfileId)
    return;

  // OnResponseStarted is even triggered, when the request was cancelled.
  if (request->status().status() != net::URLRequestStatus::SUCCESS)
    return;

  base::Time time(base::Time::Now());

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnResponseStarted, request,
                           &extra_info_spec);
  if (listeners.empty())
    return;

  // UrlRequestFileJobs do not send headers, so we simulate their behavior.
  int response_code = 200;
  if (request->response_headers())
    response_code = request->response_headers()->response_code();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetInteger(keys::kStatusCodeKey, response_code);
  dict->SetDouble(keys::kTimeStampKey, time.ToDoubleT() * 1000);
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  if (extra_info_spec & ExtraInfoSpec::STATUS_LINE)
    dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  args.Append(dict);

  DispatchEvent(profile_id, event_router, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnCompleted(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request) {
  if (profile_id == Profile::kInvalidProfileId)
    return;

  DCHECK(request->status().status() == net::URLRequestStatus::SUCCESS);

  DCHECK(!GetAndSetSignaled(request->identifier(), kOnCompleted));

  base::Time time(base::Time::Now());

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnCompleted, request,
                           &extra_info_spec);
  if (listeners.empty())
    return;

  // UrlRequestFileJobs do not send headers, so we simulate their behavior.
  int response_code = 200;
  if (request->response_headers())
    response_code = request->response_headers()->response_code();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetInteger(keys::kStatusCodeKey, response_code);
  dict->SetDouble(keys::kTimeStampKey, time.ToDoubleT() * 1000);
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  if (extra_info_spec & ExtraInfoSpec::STATUS_LINE)
    dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  args.Append(dict);

  DispatchEvent(profile_id, event_router, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnErrorOccurred(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request) {
  if (profile_id == Profile::kInvalidProfileId)
      return;

  DCHECK(request->status().status() == net::URLRequestStatus::FAILED);

  DCHECK(!GetAndSetSignaled(request->identifier(), kOnErrorOccurred));

  base::Time time(base::Time::Now());

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile_id, keys::kOnErrorOccurred, request,
                           &extra_info_spec);
  if (listeners.empty())
    return;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict->SetString(keys::kUrlKey, request->url().spec());
  dict->SetString(keys::kErrorKey,
                  net::ErrorToString(request->status().os_error()));
  dict->SetDouble(keys::kTimeStampKey, time.ToDoubleT() * 1000);
  args.Append(dict);

  DispatchEvent(profile_id, event_router, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnURLRequestDestroyed(
    ProfileId profile_id, net::URLRequest* request) {
  blocked_requests_.erase(request->identifier());
  signaled_requests_.erase(request->identifier());
  http_requests_.erase(request->identifier());
}

void ExtensionWebRequestEventRouter::OnHttpTransactionDestroyed(
    ProfileId profile_id, uint64 request_id) {
  if (blocked_requests_.find(request_id) != blocked_requests_.end() &&
      blocked_requests_[request_id].event == kOnBeforeSendHeaders) {
    // Ensure we don't call into the deleted HttpTransaction.
    blocked_requests_[request_id].callback = NULL;
    blocked_requests_[request_id].request_headers = NULL;
  }
}

bool ExtensionWebRequestEventRouter::DispatchEvent(
    ProfileId profile_id,
    ExtensionEventRouterForwarder* event_router,
    net::URLRequest* request,
    const std::vector<const EventListener*>& listeners,
    const ListValue& args) {
  std::string json_args;

  // TODO(mpcomplete): Consider consolidating common (extension_id,json_args)
  // pairs into a single message sent to a list of sub_event_names.
  int num_handlers_blocking = 0;
  for (std::vector<const EventListener*>::const_iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    // Filter out the optional keys that this listener didn't request.
    scoped_ptr<ListValue> args_filtered(args.DeepCopy());
    DictionaryValue* dict = NULL;
    CHECK(args_filtered->GetDictionary(0, &dict) && dict);
    if (!((*it)->extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS))
      dict->Remove(keys::kRequestHeadersKey, NULL);
    if (!((*it)->extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS))
      dict->Remove(keys::kResponseHeadersKey, NULL);
    if (!((*it)->extra_info_spec & ExtraInfoSpec::STATUS_LINE))
      dict->Remove(keys::kStatusLineKey, NULL);

    base::JSONWriter::Write(args_filtered.get(), false, &json_args);
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
    blocked_requests_[request->identifier()].blocking_time = base::Time::Now();

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
    EventResponse* response) {
  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  // The listener may have been removed (e.g. due to the process going away)
  // before we got here.
  std::set<EventListener>::iterator found =
      listeners_[profile_id][event_name].find(listener);
  if (found != listeners_[profile_id][event_name].end())
    found->blocked_requests.erase(request_id);

  DecrementBlockCount(request_id, response);
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

  // It's possible for AddEventListener to fail asynchronously. In that case,
  // the renderer believes the listener exists, while the browser does not.
  // Ignore a RemoveEventListener in that case.
  std::set<EventListener>::iterator found =
      listeners_[profile_id][event_name].find(listener);
  if (found == listeners_[profile_id][event_name].end())
    return;

  CHECK_EQ(listeners_[profile_id][event_name].count(listener), 1u) <<
      "extension=" << extension_id << " event=" << event_name;

  // Unblock any request that this event listener may have been blocking.
  for (std::set<uint64>::iterator it = found->blocked_requests.begin();
       it != found->blocked_requests.end(); ++it) {
    DecrementBlockCount(*it, NULL);
  }

  listeners_[profile_id][event_name].erase(listener);
}

std::vector<const ExtensionWebRequestEventRouter::EventListener*>
ExtensionWebRequestEventRouter::GetMatchingListeners(
    ProfileId profile_id,
    const std::string& event_name,
    const GURL& url,
    int tab_id,
    int window_id,
    ResourceType::Type resource_type,
    int* extra_info_spec) {
  // TODO(mpcomplete): handle profile_id == invalid (should collect all
  // listeners).
  *extra_info_spec = 0;

  std::vector<const EventListener*> matching_listeners;
  std::set<EventListener>& listeners = listeners_[profile_id][event_name];
  for (std::set<EventListener>::iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    if (!it->filter.urls.is_empty() && !it->filter.urls.MatchesURL(url))
      continue;
    if (it->filter.tab_id != -1 && tab_id != it->filter.tab_id)
      continue;
    if (it->filter.window_id != -1 && window_id != it->filter.window_id)
      continue;
    if (!it->filter.types.empty() &&
        std::find(it->filter.types.begin(), it->filter.types.end(),
                  resource_type) == it->filter.types.end())
      continue;

    matching_listeners.push_back(&(*it));
    *extra_info_spec |= it->extra_info_spec;
  }
  return matching_listeners;
}

std::vector<const ExtensionWebRequestEventRouter::EventListener*>
ExtensionWebRequestEventRouter::GetMatchingListeners(
    ProfileId profile_id,
    const std::string& event_name,
    net::URLRequest* request,
    int* extra_info_spec) {
  int tab_id = -1;
  int window_id = -1;
  ResourceType::Type resource_type = ResourceType::LAST_TYPE;
  ExtractRequestInfo(request, &tab_id, &window_id, &resource_type);

  return GetMatchingListeners(
      profile_id, event_name, request->url(), tab_id, window_id, resource_type,
      extra_info_spec);
}

void ExtensionWebRequestEventRouter::DecrementBlockCount(
    uint64 request_id,
    EventResponse* response) {
  scoped_ptr<EventResponse> response_scoped(response);

  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  if (blocked_requests_.find(request_id) == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = blocked_requests_[request_id];
  int num_handlers_blocking = --blocked_request.num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  // If |response| is NULL, then we assume the extension does not care about
  // this request. In that case, we will fall back to the previous chosen
  // response, if any. More recently installed extensions have greater
  // precedence.
  if (response) {
    if (!blocked_request.chosen_response.get() ||
        response->extension_install_time >
            blocked_request.chosen_response->extension_install_time) {
      blocked_request.chosen_response.reset(response_scoped.release());
    }
  }

  if (num_handlers_blocking == 0) {
    // TODO(mpcomplete): it would be better if we accumulated the blocking times
    // for a given request over all events.
    HISTOGRAM_TIMES("Extensions.NetworkDelay",
                     base::Time::Now() - blocked_request.blocking_time);

    if (blocked_request.event == kOnBeforeRequest) {
      CHECK(blocked_request.callback);
      if (blocked_request.chosen_response.get() &&
          !blocked_request.chosen_response->new_url.is_empty()) {
        CHECK(blocked_request.chosen_response->new_url.is_valid());
        *blocked_request.new_url = blocked_request.chosen_response->new_url;
      }
    } else if (blocked_request.event == kOnBeforeSendHeaders) {
      // It's possible that the HttpTransaction was deleted before we could call
      // the callback. In that case, we've already NULLed out the callback and
      // headers, and we just drop the response on the floor.
      if (blocked_request.chosen_response.get() &&
          blocked_request.chosen_response->request_headers.get() &&
          blocked_request.request_headers) {
        blocked_request.request_headers->Swap(
            blocked_request.chosen_response->request_headers.get());
      }
    } else {
      NOTREACHED();
    }

    // This signals a failed request to subscribers of onErrorOccurred in case
    // a request is cancelled because net::ERR_EMPTY_RESPONSE cannot be
    // distinguished from a regular failure.
    if (blocked_request.callback) {
      int rv = (blocked_request.chosen_response.get() &&
                blocked_request.chosen_response->cancel) ?
                    net::ERR_EMPTY_RESPONSE : net::OK;
      blocked_request.callback->Run(rv);
    }

    blocked_requests_.erase(request_id);
  }
}

bool ExtensionWebRequestEventRouter::GetAndSetSignaled(uint64 request_id,
                                                       EventTypes event_type) {
  SignaledRequestMap::iterator iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = (iter->second & event_type) != 0;
  iter->second |= event_type;
  return was_signaled_before;
}

void ExtensionWebRequestEventRouter::ClearSignaled(uint64 request_id,
                                                   EventTypes event_type) {
  SignaledRequestMap::iterator iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end())
    return;
  iter->second &= ~event_type;
}

bool WebRequestAddEventListener::RunImpl() {
  // Argument 0 is the callback, which we don't use here.

  ExtensionWebRequestEventRouter::RequestFilter filter;
  if (HasOptionalArgument(1)) {
    DictionaryValue* value = NULL;
    error_.clear();
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &value));
    // Failure + an empty error string means a fatal error.
    EXTENSION_FUNCTION_VALIDATE(filter.InitFromValue(*value, &error_) ||
                                !error_.empty());
    if (!error_.empty())
      return false;
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

  scoped_ptr<ExtensionWebRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(3)) {
    DictionaryValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(3, &value));

    if (!value->empty()) {
      base::Time install_time =
          profile()->GetExtensionService()->extension_prefs()->
              GetInstallTime(extension_id());
      response.reset(new ExtensionWebRequestEventRouter::EventResponse(
          extension_id(), install_time));
    }

    if (value->HasKey("cancel")) {
      // Don't allow cancel mixed with other keys.
      if (value->HasKey("redirectUrl") || value->HasKey("requestHeaders")) {
        error_ = keys::kInvalidBlockingResponse;
        return false;
      }

      bool cancel = false;
      EXTENSION_FUNCTION_VALIDATE(value->GetBoolean("cancel", &cancel));
      response->cancel = cancel;
    }

    if (value->HasKey("redirectUrl")) {
      std::string new_url_str;
      EXTENSION_FUNCTION_VALIDATE(value->GetString("redirectUrl",
                                                   &new_url_str));
      response->new_url = GURL(new_url_str);
      if (!response->new_url.is_valid()) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            keys::kInvalidRedirectUrl, new_url_str);
        return false;
      }
    }

    if (value->HasKey("requestHeaders")) {
      ListValue* request_headers_value = NULL;
      response->request_headers.reset(new net::HttpRequestHeaders());
      EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kRequestHeadersKey,
                                                 &request_headers_value));
      for (size_t i = 0; i < request_headers_value->GetSize(); ++i) {
        DictionaryValue* header_value = NULL;
        std::string name;
        std::string value;
        EXTENSION_FUNCTION_VALIDATE(
            request_headers_value->GetDictionary(i, &header_value));
        EXTENSION_FUNCTION_VALIDATE(
            header_value->GetString(keys::kHeaderNameKey, &name));
        EXTENSION_FUNCTION_VALIDATE(
            header_value->GetString(keys::kHeaderValueKey, &value));

        response->request_headers->SetHeader(name, value);
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          &EventHandledOnIOThread,
          profile()->GetRuntimeId(), extension_id(),
          event_name, sub_event_name, request_id, response.release()));

  return true;
}
