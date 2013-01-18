// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_WEB_REQUEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_WEB_REQUEST_API_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "extensions/common/url_pattern_set.h"
#include "ipc/ipc_sender.h"
#include "net/base/completion_callback.h"
#include "net/base/network_delegate.h"
#include "net/http/http_request_headers.h"
#include "webkit/glue/resource_type.h"

class ExtensionInfoMap;
class ExtensionWebRequestTimeTracker;
class GURL;

namespace base {
class DictionaryValue;
class ListValue;
class StringValue;
}

namespace content {
class RenderProcessHost;
}

namespace extensions {
class WebRequestRulesRegistry;
}

namespace net {
class AuthCredentials;
class AuthChallengeInfo;
class HttpRequestHeaders;
class HttpResponseHeaders;
class URLRequest;
}

// This class observes network events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionWebRequestEventRouter
    : public base::SupportsWeakPtr<ExtensionWebRequestEventRouter> {
 public:
  struct BlockedRequest;

  enum EventTypes {
    kInvalidEvent = 0,
    kOnBeforeRequest = 1 << 0,
    kOnBeforeSendHeaders = 1 << 1,
    kOnSendHeaders = 1 << 2,
    kOnHeadersReceived = 1 << 3,
    kOnBeforeRedirect = 1 << 4,
    kOnAuthRequired = 1 << 5,
    kOnResponseStarted = 1 << 6,
    kOnErrorOccurred = 1 << 7,
    kOnCompleted = 1 << 8,
  };

  // Internal representation of the webRequest.RequestFilter type, used to
  // filter what network events an extension cares about.
  struct RequestFilter {
    RequestFilter();
    ~RequestFilter();

    // Returns false if there was an error initializing. If it is a user error,
    // an error message is provided, otherwise the error is internal (and
    // unexpected).
    bool InitFromValue(const base::DictionaryValue& value, std::string* error);

    extensions::URLPatternSet urls;
    std::vector<ResourceType::Type> types;
    int tab_id;
    int window_id;
  };

  // Internal representation of the extraInfoSpec parameter on webRequest
  // events, used to specify extra information to be included with network
  // events.
  struct ExtraInfoSpec {
    enum Flags {
      REQUEST_HEADERS = 1<<0,
      RESPONSE_HEADERS = 1<<1,
      BLOCKING = 1<<2,
      ASYNC_BLOCKING = 1<<3,
      REQUEST_BODY = 1<<4,
    };

    static bool InitFromValue(const base::ListValue& value,
                              int* extra_info_spec);
  };

  // Contains an extension's response to a blocking event.
  struct EventResponse {
    EventResponse(const std::string& extension_id,
                  const base::Time& extension_install_time);
    ~EventResponse();

    // ID of the extension that sent this response.
    std::string extension_id;

    // The time that the extension was installed. Used for deciding order of
    // precedence in case multiple extensions respond with conflicting
    // decisions.
    base::Time extension_install_time;

    // Response values. These are mutually exclusive.
    bool cancel;
    GURL new_url;
    scoped_ptr<net::HttpRequestHeaders> request_headers;
    scoped_ptr<extension_web_request_api_helpers::ResponseHeaders>
        response_headers;

    scoped_ptr<net::AuthCredentials> auth_credentials;

    DISALLOW_COPY_AND_ASSIGN(EventResponse);
  };

  static ExtensionWebRequestEventRouter* GetInstance();

  // Registers a rule registry. Pass null for |rules_registry| to unregister
  // the rule registry for |profile|.
  void RegisterRulesRegistry(
      void* profile,
      scoped_refptr<extensions::WebRequestRulesRegistry> rules_registry);

  // Dispatches the OnBeforeRequest event to any extensions whose filters match
  // the given request. Returns net::ERR_IO_PENDING if an extension is
  // intercepting the request, OK otherwise.
  int OnBeforeRequest(void* profile,
                      ExtensionInfoMap* extension_info_map,
                      net::URLRequest* request,
                      const net::CompletionCallback& callback,
                      GURL* new_url);

  // Dispatches the onBeforeSendHeaders event. This is fired for HTTP(s)
  // requests only, and allows modification of the outgoing request headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request, OK
  // otherwise.
  int OnBeforeSendHeaders(void* profile,
                          ExtensionInfoMap* extension_info_map,
                          net::URLRequest* request,
                          const net::CompletionCallback& callback,
                          net::HttpRequestHeaders* headers);

  // Dispatches the onSendHeaders event. This is fired for HTTP(s) requests
  // only.
  void OnSendHeaders(void* profile,
                     ExtensionInfoMap* extension_info_map,
                     net::URLRequest* request,
                     const net::HttpRequestHeaders& headers);

  // Dispatches the onHeadersReceived event. This is fired for HTTP(s)
  // requests only, and allows modification of incoming response headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request,
  // OK otherwise. |original_response_headers| is reference counted. |callback|
  // and |override_response_headers| are owned by a URLRequestJob. They are
  // guaranteed to be valid until |callback| is called or OnURLRequestDestroyed
  // is called (whatever comes first).
  // Do not modify |original_response_headers| directly but write new ones
  // into |override_response_headers|.
  int OnHeadersReceived(
      void* profile,
      ExtensionInfoMap* extension_info_map,
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers);

  // Dispatches the OnAuthRequired event to any extensions whose filters match
  // the given request. If the listener is not registered as "blocking", then
  // AUTH_REQUIRED_RESPONSE_OK is returned. Otherwise,
  // AUTH_REQUIRED_RESPONSE_IO_PENDING is returned and |callback| will be
  // invoked later.
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
                     void* profile,
                     ExtensionInfoMap* extension_info_map,
                     net::URLRequest* request,
                     const net::AuthChallengeInfo& auth_info,
                     const net::NetworkDelegate::AuthCallback& callback,
                     net::AuthCredentials* credentials);

  // Dispatches the onBeforeRedirect event. This is fired for HTTP(s) requests
  // only.
  void OnBeforeRedirect(void* profile,
                        ExtensionInfoMap* extension_info_map,
                        net::URLRequest* request,
                        const GURL& new_location);

  // Dispatches the onResponseStarted event indicating that the first bytes of
  // the response have arrived.
  void OnResponseStarted(void* profile,
                         ExtensionInfoMap* extension_info_map,
                         net::URLRequest* request);

  // Dispatches the onComplete event.
  void OnCompleted(void* profile,
                   ExtensionInfoMap* extension_info_map,
                   net::URLRequest* request);

  // Dispatches an onErrorOccurred event.
  void OnErrorOccurred(void* profile,
                      ExtensionInfoMap* extension_info_map,
                      net::URLRequest* request,
                      bool started);

  // Notifications when objects are going away.
  void OnURLRequestDestroyed(void* profile, net::URLRequest* request);

  // Called when an event listener handles a blocking event and responds.
  void OnEventHandled(
      void* profile,
      const std::string& extension_id,
      const std::string& event_name,
      const std::string& sub_event_name,
      uint64 request_id,
      EventResponse* response);

  // Adds a listener to the given event. |event_name| specifies the event being
  // listened to. |sub_event_name| is an internal event uniquely generated in
  // the extension process to correspond to the given filter and
  // extra_info_spec. It returns true on success, false on failure.
  bool AddEventListener(
      void* profile,
      const std::string& extension_id,
      const std::string& extension_name,
      const std::string& event_name,
      const std::string& sub_event_name,
      const RequestFilter& filter,
      int extra_info_spec,
      base::WeakPtr<IPC::Sender> ipc_sender);

  // Removes the listener for the given sub-event.
  void RemoveEventListener(
      void* profile,
      const std::string& extension_id,
      const std::string& sub_event_name);

  // Called when an incognito profile is created or destroyed.
  void OnOTRProfileCreated(void* original_profile,
                           void* otr_profile);
  void OnOTRProfileDestroyed(void* original_profile,
                             void* otr_profile);

  // Registers a |callback| that is executed when the next page load happens.
  // The callback is then deleted.
  void AddCallbackForPageLoad(const base::Closure& callback);

 private:
  friend struct DefaultSingletonTraits<ExtensionWebRequestEventRouter>;

  struct EventListener;
  typedef std::map<std::string, std::set<EventListener> > ListenerMapForProfile;
  typedef std::map<void*, ListenerMapForProfile> ListenerMap;
  typedef std::map<uint64, BlockedRequest> BlockedRequestMap;
  // Map of request_id -> bit vector of EventTypes already signaled
  typedef std::map<uint64, int> SignaledRequestMap;
  typedef std::map<void*, void*> CrossProfileMap;
  typedef std::list<base::Closure> CallbacksForPageLoad;

  ExtensionWebRequestEventRouter();
  ~ExtensionWebRequestEventRouter();

  // Ensures that future callbacks for |request| are ignored so that it can be
  // destroyed safely.
  void ClearPendingCallbacks(net::URLRequest* request);

  bool DispatchEvent(
      void* profile,
      net::URLRequest* request,
      const std::vector<const EventListener*>& listeners,
      const base::ListValue& args);

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters. |extra_info_spec| will contain the combined
  // set of extra_info_spec flags that every matching listener asked for.
  std::vector<const EventListener*> GetMatchingListeners(
      void* profile,
      ExtensionInfoMap* extension_info_map,
      const std::string& event_name,
      net::URLRequest* request,
      int* extra_info_spec);

  // Helper for the above functions. This is called twice: once for the profile
  // of the event, the next time for the "cross" profile (i.e. the incognito
  // profile if the event is originally for the normal profile, or vice versa).
  void GetMatchingListenersImpl(
      void* profile,
      ExtensionInfoMap* extension_info_map,
      bool crosses_incognito,
      const std::string& event_name,
      const GURL& url,
      int tab_id,
      int window_id,
      ResourceType::Type resource_type,
      bool is_async_request,
      bool is_request_from_extension,
      int* extra_info_spec,
      std::vector<const ExtensionWebRequestEventRouter::EventListener*>*
          matching_listeners);

  // Decrements the count of event handlers blocking the given request. When the
  // count reaches 0, we stop blocking the request and proceed it using the
  // method requested by the extension with the highest precedence. Precedence
  // is decided by extension install time. If |response| is non-NULL, this
  // method assumes ownership.
  void DecrementBlockCount(
      void* profile,
      const std::string& extension_id,
      const std::string& event_name,
      uint64 request_id,
      EventResponse* response);

  // Processes the generated deltas from blocked_requests_ on the specified
  // request. If |call_back| is true, the callback registered in
  // |blocked_requests_| is called.
  // The function returns the error code for the network request. This is
  // mostly relevant in case the caller passes |call_callback| = false
  // and wants to return the correct network error code himself.
  int ExecuteDeltas(void* profile, uint64 request_id, bool call_callback);

  // Evaluates the rules of the declarative webrequest API and stores
  // modifications to the request that result from WebRequestActions as
  // deltas in |blocked_requests_|. |original_response_headers| should only be
  // set for the OnHeadersReceived stage and NULL otherwise. Returns whether any
  // deltas were generated.
  bool ProcessDeclarativeRules(
      void* profile,
      ExtensionInfoMap* extension_info_map,
      const std::string& event_name,
      net::URLRequest* request,
      extensions::RequestStage request_stage,
      const net::HttpResponseHeaders* original_response_headers);

  // If the BlockedRequest contains messages_to_extension entries in the event
  // deltas, we send them to subscribers of
  // chrome.declarativeWebRequest.onMessage.
  void SendMessages(void* profile, const BlockedRequest& blocked_request);

  // Called when the RulesRegistry is ready to unblock a request that was
  // waiting for said event.
  void OnRulesRegistryReady(
      void* profile,
      const std::string& event_name,
      uint64 request_id,
      extensions::RequestStage request_stage);

  // Sets the flag that |event_type| has been signaled for |request_id|.
  // Returns the value of the flag before setting it.
  bool GetAndSetSignaled(uint64 request_id, EventTypes event_type);

  // Clears the flag that |event_type| has been signaled for |request_id|.
  void ClearSignaled(uint64 request_id, EventTypes event_type);

  // Returns whether |request| represents a top level window navigation.
  bool IsPageLoad(net::URLRequest* request) const;

  // Called on a page load to process all registered callbacks.
  void NotifyPageLoad();

  // Returns the matching cross profile (the regular profile if |profile| is
  // OTR and vice versa).
  void* GetCrossProfile(void* profile) const;

  // Returns true if |request| was already signaled to some event handlers.
  bool WasSignaled(const net::URLRequest& request) const;

  // A map for each profile that maps an event name to a set of extensions that
  // are listening to that event.
  ListenerMap listeners_;

  // A map of network requests that are waiting for at least one event handler
  // to respond.
  BlockedRequestMap blocked_requests_;

  // A map of request ids to a bitvector indicating which events have been
  // signaled and should not be sent again.
  SignaledRequestMap signaled_requests_;

  // A map of original profile -> corresponding incognito profile (and vice
  // versa).
  CrossProfileMap cross_profile_map_;

  // Keeps track of time spent waiting on extensions using the blocking
  // webRequest API.
  scoped_ptr<ExtensionWebRequestTimeTracker> request_time_tracker_;

  CallbacksForPageLoad callbacks_for_page_load_;

  // Maps each profile (and OTRProfile) to its respective rules registry.
  std::map<void*, scoped_refptr<extensions::WebRequestRulesRegistry> >
      rules_registries_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestEventRouter);
};

class WebRequestAddEventListener : public SyncIOThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequestInternal.addEventListener",
                             WEBREQUESTINTERNAL_ADDEVENTLISTENER)

 protected:
  virtual ~WebRequestAddEventListener() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class WebRequestEventHandled : public SyncIOThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequestInternal.eventHandled",
                             WEBREQUESTINTERNAL_EVENTHANDLED)

 protected:
  virtual ~WebRequestEventHandled() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class WebRequestHandlerBehaviorChangedFunction
    : public SyncIOThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequest.handlerBehaviorChanged",
                             WEBREQUEST_HANDLERBEHAVIORCHANGED)

 protected:
  virtual ~WebRequestHandlerBehaviorChangedFunction() {}

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
  // Handle quota exceeded gracefully: Only warn the user but still execute the
  // function.
  virtual void OnQuotaExceeded(const std::string& error) OVERRIDE;
  virtual bool RunImpl() OVERRIDE;
};

// Send updates to |host| with information about what webRequest-related
// extensions are installed.
// TODO(mpcomplete): remove. http://crbug.com/100411
void SendExtensionWebRequestStatusToHost(content::RenderProcessHost* host);

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_WEB_REQUEST_API_H_
