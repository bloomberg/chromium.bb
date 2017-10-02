// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "net/base/host_port_pair.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

class FrameTreeNode;
class NavigationHandle;
class NavigationHandleImpl;
class RenderFrameHost;
class TestRenderFrameHost;
struct Referrer;

// An interface for simulating a navigation in unit tests. Currently this only
// supports renderer-initiated navigations.
// Note: this should not be used in browser tests.
// TODO(clamy): support browser-initiated navigations.
class NavigationSimulator : public WebContentsObserver {
 public:
  // Simulates a browser-initiated navigation to |url| started in
  // |web_contents| from start to commit. Returns the RenderFrameHost that
  // committed the navigation.
  static RenderFrameHost* NavigateAndCommitFromBrowser(
      WebContents* web_contents,
      const GURL& url);

  // Simulates a renderer-initiated navigation to |url| started in
  // |render_frame_host| from start to commit. Returns the RenderFramehost that
  // committed the navigation.
  static RenderFrameHost* NavigateAndCommitFromDocument(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  // Simulates a failed browser-initiated navigation to |url| started in
  // |web_contents| from start to commit. Returns the RenderFrameHost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* NavigateAndFailFromBrowser(WebContents* web_contents,
                                                     const GURL& url,
                                                     int net_error_code);

  // Simulates a failed renderer-initiated navigation to |url| started in
  // |render_frame_host| from start to commit. Returns the RenderFramehost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* NavigateAndFailFromDocument(
      const GURL& original_url,
      int net_error_code,
      RenderFrameHost* render_frame_host);

  // ---------------------------------------------------------------------------

  // All the following methods should be used when more precise control over the
  // navigation is needed.

  // Creates a NavigationSimulator that will be used to simulate a
  // browser-initiated navigation to |original_url| started in |contents|.
  static std::unique_ptr<NavigationSimulator> CreateBrowserInitiated(
      const GURL& original_url,
      WebContents* contents);

  // Creates a NavigationSimulator that will be used to simulate a
  // renderer-initiated navigation to |original_url| started by
  // |render_frame_host|.
  static std::unique_ptr<NavigationSimulator> CreateRendererInitiated(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  ~NavigationSimulator() override;

  // --------------------------------------------------------------------------

  // The following functions should be used to simulate events happening during
  // a navigation.
  //
  // Example of usage for a successful renderer-initiated navigation:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  //   simulator->Start();
  //   for (GURL redirect_url : redirects)
  //     simulator->Redirect(redirect_url);
  //   simulator->Commit();
  //
  // Example of usage for a failed renderer-initiated navigation:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  //   simulator->Start();
  //   for (GURL redirect_url : redirects)
  //     simulator->Redirect(redirect_url);
  //   simulator->Fail(net::ERR_TIMED_OUT);
  //   simulator->CommitErrorPage();
  //
  // Example of usage for a same-page renderer-initiated navigation:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->CommitSameDocument();
  //
  // Example of usage for a renderer-initiated navigation which is cancelled by
  // a throttle upon redirecting. Note that registering the throttle is done
  // elsewhere:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  //   simulator->Start();
  //   simulator->Redirect(redirect_url);
  //   EXPECT_EQ(NavigationThrottle::CANCEL,
  //             simulator->GetLastThrottleCheckResult());

  // Simulates the start of the navigation.
  virtual void Start();

  // Simulates a redirect to |new_url| for the navigation.
  virtual void Redirect(const GURL& new_url);

  // Simulates receiving the navigation response and choosing a final
  // RenderFrameHost to commit it.
  virtual void ReadyToCommit();

  // Simulates the commit of the navigation in the RenderFrameHost.
  virtual void Commit();

  // Simulates the navigation failing with the error code |error_code|.
  virtual void Fail(int error_code);

  // Simulates the commit of an error page following a navigation failure.
  virtual void CommitErrorPage();

  // Simulates the commit of a same-document navigation, ie fragment navigations
  // or pushState/popState navigations.
  virtual void CommitSameDocument();

  // Must be called after the simulated navigation or an error page has
  // committed. Returns the RenderFrameHost the navigation committed in.
  virtual RenderFrameHost* GetFinalRenderFrameHost();

  // --------------------------------------------------------------------------

  // The following functions are used to specify the parameters of the
  // navigation.

  // The following parameters are constant during the navigation and may only be
  // specified before calling |Start|.
  virtual void SetTransition(ui::PageTransition transition);
  virtual void SetHasUserGesture(bool has_user_gesture);

  // The following parameters can change during redirects. They should be
  // specified before calling |Start| if they need to apply to the navigation to
  // the original url. Otherwise, they should be specified before calling
  // |Redirect|.
  virtual void SetReferrer(const Referrer& referrer);

  // The following parameters can change at any point until the page fails or
  // commits. They should be specified before calling |Fail| or |Commit|.
  virtual void SetSocketAddress(const net::HostPortPair& socket_address);

  // --------------------------------------------------------------------------

  // Gets the last throttle check result computed by the navigation throttles.
  // It is an error to call this before Start() is called.
  virtual NavigationThrottle::ThrottleCheckResult GetLastThrottleCheckResult();

  // Returns the NavigationHandle associated with the navigation being
  // simulated. It is an error to call this before Start() or after the
  // navigation has finished (successfully or not).
  virtual NavigationHandle* GetNavigationHandle() const;

  // Returns the GlobalRequestID for the simulated navigation request. Can be
  // invoked after the navigation has completed. It is an error to call this
  // before the simulated navigation has completed its WillProcessResponse
  // callback.
  content::GlobalRequestID GetGlobalRequestID() const;

  // Allows the user of the NavigationSimulator to specify a callback that will
  // be called if the navigation is deferred by a NavigationThrottle. This is
  // used for testing deferring NavigationThrottles.
  //
  // Example usage:
  //   void CheckThrottleStateAndResume() {
  //     // Do some testing here.
  //     deferring_navigation_throttle->Resume();
  //   }
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetOnDeferCallback(base::Bind(&CheckThrottleStateAndResume));
  //   simulator->Start();
  //   simulator->Commit();
  void SetOnDeferCallback(const base::Closure& on_defer_callback);

 private:
  NavigationSimulator(const GURL& original_url,
                      bool browser_initiated,
                      WebContentsImpl* web_contents,
                      TestRenderFrameHost* render_frame_host);

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void OnWillStartRequest();
  void OnWillRedirectRequest();
  void OnWillProcessResponse();

  // Simulates a browser-initiated navigation starting. Returns false if the
  // navigation failed synchronously.
  bool SimulateBrowserInitiatedStart();

  // Simulates a renderer-initiated navigation starting. Returns false if the
  // navigation failed synchronously.
  bool SimulateRendererInitiatedStart();

  // This method will block waiting for throttle checks to complete.
  void WaitForThrottleChecksComplete();

  // Sets |last_throttle_check_result_| and calls
  // |throttle_checks_wait_closure_|.
  void OnThrottleChecksComplete(NavigationThrottle::ThrottleCheckResult result);

  // Helper method to set the OnThrottleChecksComplete callback on the
  // NavigationHandle.
  void PrepareCompleteCallbackOnHandle();

  // Simulates the DidFailProvisionalLoad IPC following a NavigationThrottle
  // cancelling the navigation.
  // PlzNavigate: this is not needed.
  void FailFromThrottleCheck(NavigationThrottle::ThrottleCheckResult result);

  // Check if the navigation corresponds to a same-document navigation.
  // PlzNavigate: only use on renderer-initiated navigations.
  bool CheckIfSameDocument();

  enum State {
    INITIALIZATION,
    STARTED,
    READY_TO_COMMIT,
    FAILED,
    FINISHED,
  };

  State state_ = INITIALIZATION;

  // The WebContents in which the navigation is taking place.
  WebContentsImpl* web_contents_;

  // The renderer associated with this navigation.
  // Note: this can initially be null for browser-initiated navigations.
  TestRenderFrameHost* render_frame_host_;

  FrameTreeNode* frame_tree_node_;

  // The NavigationHandle associated with this navigation.
  NavigationHandleImpl* handle_;

  GURL navigation_url_;
  net::HostPortPair socket_address_;
  bool browser_initiated_;
  bool same_document_ = false;
  Referrer referrer_;
  ui::PageTransition transition_;
  bool has_user_gesture_ = true;

  // These are used to sanity check the content/public/ API calls emitted as
  // part of the navigation.
  int num_did_start_navigation_called_ = 0;
  int num_will_start_request_called_ = 0;
  int num_will_redirect_request_called_ = 0;
  int num_did_redirect_navigation_called_ = 0;
  int num_will_process_response_called_ = 0;
  int num_ready_to_commit_called_ = 0;
  int num_did_finish_navigation_called_ = 0;

  // Holds the last ThrottleCheckResult calculated by the navigation's
  // throttles. Will be unset before WillStartRequest is finished. Will be unset
  // while throttles are being run, but before they finish.
  base::Optional<NavigationThrottle::ThrottleCheckResult>
      last_throttle_check_result_;

  // GlobalRequestID for the associated NavigationHandle. Only valid after
  // WillProcessResponse has been invoked on the NavigationHandle.
  content::GlobalRequestID request_id_;

  // Closure that is set when WaitForThrottleChecksComplete is called.
  base::Closure throttle_checks_wait_closure_;

  // Temporarily holds a closure that will be called on navigation deferral
  // until the NavigationHandle for this navigation has been created.
  base::Closure on_defer_callback_;

  base::WeakPtrFactory<NavigationSimulator> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
