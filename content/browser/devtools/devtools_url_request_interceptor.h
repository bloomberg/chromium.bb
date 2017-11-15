// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_REQUEST_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_REQUEST_INTERCEPTOR_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_target_registry.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/common/resource_type.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_interceptor.h"

namespace content {
namespace protocol {
class NetworkHandler;
}  // namespace

class BrowserContext;
class DevToolsInterceptorController;
class DevToolsTargetRegistry;
class DevToolsURLInterceptorRequestJob;
class ResourceRequestInfo;

struct InterceptedRequestInfo {
  InterceptedRequestInfo();
  ~InterceptedRequestInfo();

  std::string interception_id;
  std::unique_ptr<protocol::Network::Request> network_request;
  base::UnguessableToken frame_id;
  ResourceType resource_type;
  bool is_navigation;
  protocol::Maybe<protocol::Object> headers_object;
  protocol::Maybe<int> http_status_code;
  protocol::Maybe<protocol::String> redirect_url;
  protocol::Maybe<protocol::Network::AuthChallenge> auth_challenge;
  int response_error_code;
  protocol::Maybe<int> http_response_status_code;
  protocol::Maybe<protocol::Object> response_headers;
};

// An interceptor that creates DevToolsURLInterceptorRequestJobs for requests
// from pages where interception has been enabled via
// Network.setRequestInterceptionEnabled.
// This class is constructed on the UI thread but only accessed on IO thread.
class DevToolsURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  static bool IsNavigationRequest(ResourceType resource_type);

  explicit DevToolsURLRequestInterceptor(BrowserContext* browser_context);
  ~DevToolsURLRequestInterceptor() override;

  using ContinueInterceptedRequestCallback =
      protocol::Network::Backend::ContinueInterceptedRequestCallback;
  using GetResponseBodyForInterceptionCallback =
      protocol::Network::Backend::GetResponseBodyForInterceptionCallback;

  struct Modifications {
    Modifications(base::Optional<net::Error> error_reason,
                  base::Optional<std::string> raw_response,
                  protocol::Maybe<std::string> modified_url,
                  protocol::Maybe<std::string> modified_method,
                  protocol::Maybe<std::string> modified_post_data,
                  protocol::Maybe<protocol::Network::Headers> modified_headers,
                  protocol::Maybe<protocol::Network::AuthChallengeResponse>
                      auth_challenge_response,
                  bool mark_as_canceled);
    ~Modifications();

    bool RequestShouldContineUnchanged() const;

    // If none of the following are set then the request will be allowed to
    // continue unchanged.
    base::Optional<net::Error> error_reason;   // Finish with error.
    base::Optional<std::string> raw_response;  // Finish with mock response.

    // Optionally modify before sending to network.
    protocol::Maybe<std::string> modified_url;
    protocol::Maybe<std::string> modified_method;
    protocol::Maybe<std::string> modified_post_data;
    protocol::Maybe<protocol::Network::Headers> modified_headers;

    // AuthChallengeResponse is mutually exclusive with the above.
    protocol::Maybe<protocol::Network::AuthChallengeResponse>
        auth_challenge_response;

    bool mark_as_canceled;
  };

  enum InterceptionStage {
    REQUEST,
    RESPONSE,
    // Note: Both is not sent from front-end. It is used if both Request
    // and HeadersReceived was found it upgrades it to Both.
    BOTH,
    DONT_INTERCEPT
  };

  struct Pattern {
   public:
    Pattern();
    ~Pattern();
    Pattern(const Pattern& other);
    Pattern(const std::string& url_pattern,
            base::flat_set<ResourceType> resource_types,
            InterceptionStage interception_stage);
    const std::string url_pattern;
    const base::flat_set<ResourceType> resource_types;
    InterceptionStage interception_stage;
  };

  struct InterceptedPage {
    InterceptedPage(base::WeakPtr<protocol::NetworkHandler> network_handler,
                    std::vector<Pattern> intercepted_patterns);
    ~InterceptedPage();

    const base::WeakPtr<protocol::NetworkHandler> network_handler;
    const std::vector<Pattern> intercepted_patterns;
  };

  // net::URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override;

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  // Registers a |sub_request| that should not be intercepted.
  void RegisterSubRequest(const net::URLRequest* sub_request);

  // Unregisters a |sub_request|.
  void UnregisterSubRequest(const net::URLRequest* sub_request);

  // To make the user's life easier we make sure requests in a redirect chain
  // all have the same id.
  void ExpectRequestAfterRedirect(const net::URLRequest* request,
                                  std::string id);

  void JobFinished(const std::string& interception_id, bool is_navigation);
  void GetResponseBody(
      std::string interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback);
  void ContinueInterceptedRequest(
      std::string interception_id,
      std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
          modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);

  void StartInterceptingRequests(
      const base::UnguessableToken& target_id,
      std::unique_ptr<InterceptedPage> interceptedPage);

  void StopInterceptingRequests(const base::UnguessableToken& target_id);

 private:
  net::URLRequestJob* InnerMaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate);

  const DevToolsTargetRegistry::TargetInfo* TargetInfoForRequestInfo(
      const ResourceRequestInfo* request_info) const;

  std::string GetIdForRequest(const net::URLRequest* request,
                              bool* is_redirect);

  // Returns a WeakPtr to the DevToolsURLInterceptorRequestJob corresponding
  // to |interception_id|.
  DevToolsURLInterceptorRequestJob* GetJob(
      const std::string& interception_id) const;

  std::unique_ptr<DevToolsTargetRegistry::Resolver> target_resolver_;
  base::WeakPtr<DevToolsInterceptorController> controller_;

  base::flat_map<base::UnguessableToken, std::unique_ptr<InterceptedPage>>
      target_id_to_intercepted_page_;

  base::flat_map<std::string, DevToolsURLInterceptorRequestJob*>
      interception_id_to_job_map_;

  // The DevToolsURLInterceptorRequestJob proxies a sub request to actually
  // fetch the bytes from the network. We don't want to intercept those
  // requests.
  base::flat_set<const net::URLRequest*> sub_requests_;

  // To simplify handling of redirect chains for the end user, we arrange for
  // all requests in the chain to have the same interception id.
  base::flat_map<const net::URLRequest*, std::string> expected_redirects_;
  size_t next_id_;

  base::WeakPtrFactory<DevToolsURLRequestInterceptor> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsURLRequestInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_REQUEST_INTERCEPTOR_H_
