// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_REQUEST_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_REQUEST_INTERCEPTOR_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_type.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_interceptor.h"

namespace content {
namespace protocol {
class NetworkHandler;
}  // namespace

class BrowserContext;
class DevToolsURLInterceptorRequestJob;
class WebContents;
class RenderFrameHost;

// An interceptor that creates DevToolsURLInterceptorRequestJobs for requests
// from pages where interception has been enabled via
// Network.setRequestInterceptionEnabled.
class DevToolsURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit DevToolsURLRequestInterceptor(BrowserContext* browser_context);
  ~DevToolsURLRequestInterceptor() override;

  // Must be called on UI thread.
  static DevToolsURLRequestInterceptor* FromBrowserContext(
      BrowserContext* context);

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

  struct Pattern {
   public:
    Pattern();
    ~Pattern();
    Pattern(const Pattern& other);
    Pattern(const std::string& url_pattern,
            base::flat_set<ResourceType> resource_types);
    const std::string url_pattern;
    const base::flat_set<ResourceType> resource_types;
  };

  // The State needs to be accessed on both UI and IO threads.
  class State : public base::RefCountedThreadSafe<State> {
   public:
    State();

    using FrameTreeNodeId = int;
    using ContinueInterceptedRequestCallback =
        protocol::Network::Backend::ContinueInterceptedRequestCallback;

    // Must be called on the UI thread.
    void ContinueInterceptedRequest(
        std::string interception_id,
        std::unique_ptr<Modifications> modifications,
        std::unique_ptr<ContinueInterceptedRequestCallback> callback);

    // Returns a DevToolsURLInterceptorRequestJob if the request should be
    // intercepted, otherwise returns nullptr. Must be called on the IO thread.
    DevToolsURLInterceptorRequestJob*
    MaybeCreateDevToolsURLInterceptorRequestJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate);

    // Must be called on the UI thread.
    void StartInterceptingRequests(
        WebContents* web_contents,
        base::WeakPtr<protocol::NetworkHandler> network_handler,
        std::vector<Pattern> intercepted_patterns);

    // Must be called on the UI thread.
    void StopInterceptingRequests(WebContents* web_contents);

    // Registers a |sub_request| that should not be intercepted.
    void RegisterSubRequest(const net::URLRequest* sub_request);

    // Unregisters a |sub_request|. Must be called on the IO thread.
    void UnregisterSubRequest(const net::URLRequest* sub_request);

    // To make the user's life easier we make sure requests in a redirect chain
    // all have the same id. Must be called on the IO thread.
    void ExpectRequestAfterRedirect(const net::URLRequest* request,
                                    std::string id);

    // Must be called on the IO thread.
    void JobFinished(const std::string& interception_id);

   private:
    class InterceptedWebContentsObserver;

    struct RenderFrameHostInfo {
      explicit RenderFrameHostInfo(RenderFrameHost* host);
      const int routing_id;
      const FrameTreeNodeId frame_tree_node_id;
      const int process_id;
    };

    struct InterceptedPage {
      InterceptedPage(base::WeakPtr<protocol::NetworkHandler> network_handler,
                      std::vector<Pattern> intercepted_patterns);
      ~InterceptedPage();

      const base::WeakPtr<protocol::NetworkHandler> network_handler;
      const std::vector<Pattern> intercepted_patterns;
    };

    void ContinueInterceptedRequestOnIO(
        std::string interception_id,
        std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
            modifications,
        std::unique_ptr<ContinueInterceptedRequestCallback> callback);

    void RenderFrameHostChangedOnIO(
        base::Optional<RenderFrameHostInfo> old_host_info,
        RenderFrameHostInfo new_host_info,
        WebContents* web_contents);

    void StartInterceptingRequestsForHostInfoOnIOInternal(
        RenderFrameHostInfo host_info,
        WebContents* web_contents);

    void StartInterceptingRequestsOnIO(
        std::vector<RenderFrameHostInfo> host_info_list,
        WebContents* web_contents,
        std::unique_ptr<InterceptedPage> interceptedPage);

    void StopInterceptingRequestsForHostInfoOnIO(RenderFrameHostInfo host_info);
    void StopInterceptingRequestsOnIO(WebContents* web_contents);

    std::string GetIdForRequestOnIO(const net::URLRequest* request,
                                    bool* is_redirect);

    // Returns a WeakPtr to the DevToolsURLInterceptorRequestJob corresponding
    // to |interception_id|.  Must be called on the IO thread.
    DevToolsURLInterceptorRequestJob* GetJob(
        const std::string& interception_id) const;

    // |intercepted_page_for_web_contents_| should always have an entry if
    // |intercepted_render_frames_| or |intercepted_frame_tree_nodes_| have
    // values. Entries in |intercepted_page_for_web_contents_| only get removed
    // if WebContents goes away.
    base::flat_map<WebContents*, std::unique_ptr<InterceptedPage>>
        intercepted_page_for_web_contents_;
    // First item is routing_id second is process_id.
    base::flat_map<std::pair<int, int>, WebContents*>
        intercepted_render_frames_;
    base::flat_map<FrameTreeNodeId, WebContents*> intercepted_frame_tree_nodes_;

    // UI thread only.
    base::flat_map<WebContents*,
                   std::unique_ptr<InterceptedWebContentsObserver>>
        observers_;

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

   private:
    friend class base::RefCountedThreadSafe<State>;
    ~State();
    DISALLOW_COPY_AND_ASSIGN(State);
  };

  const scoped_refptr<State>& state() const { return state_; }

 private:
  BrowserContext* const browser_context_;

  scoped_refptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLRequestInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_REQUEST_INTERCEPTOR_H_
