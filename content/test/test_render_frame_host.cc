// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include "base/guid.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/frame_policy.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/platform/WebMixedContentContextType.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "ui/base/page_transition_types.h"

namespace content {

TestRenderFrameHostCreationObserver::TestRenderFrameHostCreationObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents), last_created_frame_(NULL) {
}

TestRenderFrameHostCreationObserver::~TestRenderFrameHostCreationObserver() {
}

void TestRenderFrameHostCreationObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  last_created_frame_ = render_frame_host;
}

TestRenderFrameHost::TestRenderFrameHost(SiteInstance* site_instance,
                                         RenderViewHostImpl* render_view_host,
                                         RenderFrameHostDelegate* delegate,
                                         RenderWidgetHostDelegate* rwh_delegate,
                                         FrameTree* frame_tree,
                                         FrameTreeNode* frame_tree_node,
                                         int32_t routing_id,
                                         int32_t widget_routing_id,
                                         int flags)
    : RenderFrameHostImpl(site_instance,
                          render_view_host,
                          delegate,
                          rwh_delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          widget_routing_id,
                          flags,
                          false),
      child_creation_observer_(delegate ? delegate->GetAsWebContents() : NULL),
      contents_mime_type_("text/html"),
      simulate_history_list_was_cleared_(false),
      last_commit_was_error_page_(false) {}

TestRenderFrameHost::~TestRenderFrameHost() {
}

TestRenderViewHost* TestRenderFrameHost::GetRenderViewHost() {
  return static_cast<TestRenderViewHost*>(
      RenderFrameHostImpl::GetRenderViewHost());
}

MockRenderProcessHost* TestRenderFrameHost::GetProcess() {
  return static_cast<MockRenderProcessHost*>(RenderFrameHostImpl::GetProcess());
}

void TestRenderFrameHost::InitializeRenderFrameIfNeeded() {
  if (!render_view_host()->IsRenderViewLive()) {
    render_view_host()->GetProcess()->Init();
    RenderViewHostTester::For(render_view_host())->CreateTestRenderView(
        base::string16(), MSG_ROUTING_NONE, MSG_ROUTING_NONE, false);
  }
}

TestRenderFrameHost* TestRenderFrameHost::AppendChild(
    const std::string& frame_name) {
  std::string frame_unique_name = base::GenerateGUID();
  OnCreateChildFrame(GetProcess()->GetNextRoutingID(),
                     blink::WebTreeScopeType::kDocument, frame_name,
                     frame_unique_name, base::UnguessableToken::Create(),
                     FramePolicy(), FrameOwnerProperties());
  return static_cast<TestRenderFrameHost*>(
      child_creation_observer_.last_created_frame());
}

void TestRenderFrameHost::Detach() {
  OnDetach();
}

void TestRenderFrameHost::SimulateNavigationStart(const GURL& url) {
  if (IsBrowserSideNavigationEnabled()) {
    SendRendererInitiatedNavigationRequest(url, false);
    return;
  }

  OnDidStartLoading(true);
  OnDidStartProvisionalLoad(url, std::vector<GURL>(), base::TimeTicks::Now());
  SimulateWillStartRequest(ui::PAGE_TRANSITION_LINK);
}

void TestRenderFrameHost::SimulateRedirect(const GURL& new_url) {
  if (IsBrowserSideNavigationEnabled()) {
    NavigationRequest* request = frame_tree_node_->navigation_request();
    if (!request->loader_for_testing()) {
      base::RunLoop loop;
      request->set_on_start_checks_complete_closure_for_testing(
          loop.QuitClosure());
      loop.Run();
    }
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    CHECK(url_loader);
    url_loader->SimulateServerRedirect(new_url);
    return;
  }

  navigation_handle()->CallWillRedirectRequestForTesting(new_url, false, GURL(),
                                                         false);
}

void TestRenderFrameHost::SimulateNavigationCommit(const GURL& url) {
  if (frame_tree_node()->navigation_request())
    PrepareForCommit();

  bool is_auto_subframe =
      GetParent() && !frame_tree_node()->has_committed_real_load();

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.url = url;
  params.origin = url::Origin(url);
  if (!GetParent())
    params.transition = ui::PAGE_TRANSITION_LINK;
  else if (is_auto_subframe)
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  else
    params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = true;
  params.did_create_new_entry = !is_auto_subframe;
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.method = "GET";
  params.http_status_code = 200;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = url;

  url::Replacements<char> replacements;
  replacements.ClearRef();

  // This approach to determining whether a navigation is to be treated as
  // same document is not robust, as it will not handle pushState type
  // navigation. Do not use elsewhere!
  params.was_within_same_document =
      (GetLastCommittedURL().is_valid() && !last_commit_was_error_page_ &&
       url.ReplaceComponents(replacements) ==
           GetLastCommittedURL().ReplaceComponents(replacements));

  params.page_state = PageState::CreateForTesting(url, false, nullptr, nullptr);

  SendNavigateWithParams(&params);
}

void TestRenderFrameHost::SimulateNavigationError(const GURL& url,
                                                  int error_code) {
  if (IsBrowserSideNavigationEnabled()) {
    NavigationRequest* request = frame_tree_node_->navigation_request();
    CHECK(request);
    // Simulate a beforeUnload ACK from the renderer if the browser is waiting
    // for it. If it runs it will update the request state.
    if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
      static_cast<TestRenderFrameHost*>(frame_tree_node()->current_frame_host())
          ->SendBeforeUnloadACK(true);
    }
    if (!request->loader_for_testing()) {
      base::RunLoop loop;
      request->set_on_start_checks_complete_closure_for_testing(
          loop.QuitClosure());
      loop.Run();
    }
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    CHECK(url_loader);
    url_loader->SimulateError(error_code);
    return;
  }

  FrameHostMsg_DidFailProvisionalLoadWithError_Params error_params;
  error_params.error_code = error_code;
  error_params.url = url;
  OnDidFailProvisionalLoadWithError(error_params);
}

void TestRenderFrameHost::SimulateNavigationErrorPageCommit() {
  CHECK(navigation_handle());
  GURL error_url = GURL(kUnreachableWebDataURL);
  OnDidStartProvisionalLoad(error_url, std::vector<GURL>(),
                            base::TimeTicks::Now());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = navigation_handle()->GetURL();
  params.transition = GetParent() ? ui::PAGE_TRANSITION_MANUAL_SUBFRAME
                                  : ui::PAGE_TRANSITION_LINK;
  params.was_within_same_document = false;
  params.url_is_unreachable = true;
  params.page_state = PageState::CreateForTesting(navigation_handle()->GetURL(),
                                                  false, nullptr, nullptr);
  SendNavigateWithParams(&params);
}

void TestRenderFrameHost::SimulateNavigationStop() {
  if (is_loading()) {
    OnDidStopLoading();
  } else if (IsBrowserSideNavigationEnabled()) {
    // Even if the RenderFrameHost is not loading, there may still be an
    // ongoing navigation in the FrameTreeNode. Cancel this one as well.
    frame_tree_node()->ResetNavigationRequest(false, true);
  }
}

void TestRenderFrameHost::SetContentsMimeType(const std::string& mime_type) {
  contents_mime_type_ = mime_type;
}

void TestRenderFrameHost::SendBeforeUnloadACK(bool proceed) {
  base::TimeTicks now = base::TimeTicks::Now();
  OnBeforeUnloadACK(proceed, now, now);
}

void TestRenderFrameHost::SimulateSwapOutACK() {
  OnSwappedOut();
}

void TestRenderFrameHost::NavigateAndCommitRendererInitiated(
    bool did_create_new_entry,
    const GURL& url) {
  SendRendererInitiatedNavigationRequest(url, false);
  // PlzNavigate: If no network request is needed by the navigation, then there
  // will be no NavigationRequest, nor is it necessary to simulate the network
  // stack commit.
  if (frame_tree_node()->navigation_request())
    PrepareForCommit();
  bool browser_side_navigation = IsBrowserSideNavigationEnabled();
  CHECK(!browser_side_navigation || is_loading());
  CHECK(!browser_side_navigation || !frame_tree_node()->navigation_request());
  SendNavigate(0, did_create_new_entry, url);
}

void TestRenderFrameHost::SimulateFeaturePolicyHeader(
    blink::WebFeaturePolicyFeature feature,
    const std::vector<url::Origin>& whitelist) {
  content::ParsedFeaturePolicyHeader header(1);
  header[0].feature = feature;
  header[0].matches_all_origins = false;
  header[0].origins = whitelist;
  OnDidSetFeaturePolicyHeader(header);
}

void TestRenderFrameHost::SendNavigate(int nav_entry_id,
                                       bool did_create_new_entry,
                                       const GURL& url) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, false,
                             url, ui::PAGE_TRANSITION_LINK, 200,
                             ModificationCallback());
}

void TestRenderFrameHost::SendFailedNavigate(int nav_entry_id,
                                             bool did_create_new_entry,
                                             const GURL& url) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, false,
                             url, ui::PAGE_TRANSITION_RELOAD, 500,
                             ModificationCallback());
}

void TestRenderFrameHost::SendNavigateWithTransition(
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    ui::PageTransition transition) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, false,
                             url, transition, 200, ModificationCallback());
}

void TestRenderFrameHost::SendNavigateWithReplacement(int nav_entry_id,
                                                      bool did_create_new_entry,
                                                      const GURL& url) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, true,
                             url, ui::PAGE_TRANSITION_LINK, 200,
                             ModificationCallback());
}

void TestRenderFrameHost::SendNavigateWithModificationCallback(
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    const ModificationCallback& callback) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, false,
                             url, ui::PAGE_TRANSITION_LINK, 200, callback);
}

void TestRenderFrameHost::SendNavigateWithParameters(
    int nav_entry_id,
    bool did_create_new_entry,
    bool should_replace_entry,
    const GURL& url,
    ui::PageTransition transition,
    int response_code,
    const ModificationCallback& callback) {
  if (!IsBrowserSideNavigationEnabled())
    OnDidStartLoading(true);

  // DidStartProvisionalLoad may delete the pending entry that holds |url|,
  // so we keep a copy of it to use below.
  GURL url_copy(url);
  OnDidStartProvisionalLoad(url_copy, std::vector<GURL>(),
                            base::TimeTicks::Now());
  SimulateWillStartRequest(transition);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = nav_entry_id;
  params.url = url_copy;
  params.transition = transition;
  params.should_update_history = true;
  params.did_create_new_entry = did_create_new_entry;
  params.should_replace_current_entry = should_replace_entry;
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.method = "GET";
  params.http_status_code = response_code;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = url_copy;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params.item_sequence_number = base::Time::Now().ToDoubleT() * 1000000;
  params.document_sequence_number = params.item_sequence_number + 1;

  // When the user hits enter in the Omnibox without changing the URL, Blink
  // behaves similarly to a reload and does not change the item and document
  // sequence numbers. Simulate this behavior here too.
  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    const NavigationEntryImpl* entry =
        static_cast<NavigationEntryImpl*>(frame_tree_node()
                                              ->navigator()
                                              ->GetController()
                                              ->GetLastCommittedEntry());
    if (entry && entry->GetURL() == url) {
      FrameNavigationEntry* frame_entry =
          entry->GetFrameEntry(frame_tree_node());
      if (frame_entry) {
        params.item_sequence_number = frame_entry->item_sequence_number();
        params.document_sequence_number =
            frame_entry->document_sequence_number();
      }
    }
  }

  // In most cases, the origin will match the URL's origin.  Tests that need to
  // check corner cases (like about:blank) should specify the origin param
  // manually.
  url::Origin origin(url_copy);
  params.origin = origin;

  url::Replacements<char> replacements;
  replacements.ClearRef();

  // This approach to determining whether a navigation is to be treated as
  // same document is not robust, as it will not handle pushState type
  // navigation. Do not use elsewhere!
  params.was_within_same_document =
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) &&
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
      (GetLastCommittedURL().is_valid() && !last_commit_was_error_page_ &&
       url_copy.ReplaceComponents(replacements) ==
           GetLastCommittedURL().ReplaceComponents(replacements));

  params.page_state =
      PageState::CreateForTestingWithSequenceNumbers(
          url_copy, params.item_sequence_number,
          params.document_sequence_number);

  if (!callback.is_null())
    callback.Run(&params);

  SendNavigateWithParams(&params);
}

void TestRenderFrameHost::SendNavigateWithParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  if (navigation_handle()) {
    scoped_refptr<net::HttpResponseHeaders> response_headers =
        new net::HttpResponseHeaders(std::string());
    response_headers->AddHeader(
        std::string("Content-Type: ") + contents_mime_type_);
    navigation_handle()->set_response_headers_for_testing(response_headers);
  }
  DidCommitProvisionalLoad(
      base::MakeUnique<FrameHostMsg_DidCommitProvisionalLoad_Params>(*params));
  last_commit_was_error_page_ = params->url_is_unreachable;
}

void TestRenderFrameHost::SendRendererInitiatedNavigationRequest(
    const GURL& url,
    bool has_user_gesture) {
  // Since this is renderer-initiated navigation, the RenderFrame must be
  // initialized. Do it if it hasn't happened yet.
  InitializeRenderFrameIfNeeded();

  if (IsBrowserSideNavigationEnabled()) {
    // TODO(mkwst): The initiator origin here is incorrect.
    BeginNavigationParams begin_params(
        std::string(), net::LOAD_NORMAL, has_user_gesture, false,
        REQUEST_CONTEXT_TYPE_HYPERLINK,
        blink::WebMixedContentContextType::kBlockable,
        false,  // is_form_submission
        url::Origin());
    CommonNavigationParams common_params;
    common_params.url = url;
    common_params.referrer = Referrer(GURL(), blink::kWebReferrerPolicyDefault);
    common_params.transition = ui::PAGE_TRANSITION_LINK;
    common_params.navigation_type = FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
    OnBeginNavigation(common_params, begin_params);
  }
}

void TestRenderFrameHost::DidChangeOpener(int opener_routing_id) {
  OnDidChangeOpener(opener_routing_id);
}

void TestRenderFrameHost::DidEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  OnEnforceInsecureRequestPolicy(policy);
}

void TestRenderFrameHost::PrepareForCommit() {
  PrepareForCommitWithServerRedirect(GURL());
}

void TestRenderFrameHost::PrepareForCommitWithServerRedirect(
    const GURL& redirect_url) {
  if (!IsBrowserSideNavigationEnabled()) {
    // Non PlzNavigate
    if (is_waiting_for_beforeunload_ack())
      SendBeforeUnloadACK(true);
    return;
  }

  // PlzNavigate
  NavigationRequest* request = frame_tree_node_->navigation_request();
  CHECK(request);
  bool have_to_make_network_request =
      IsURLHandledByNetworkStack(request->common_params().url) &&
      !FrameMsg_Navigate_Type::IsSameDocument(
          request->common_params().navigation_type);

  // Simulate a beforeUnload ACK from the renderer if the browser is waiting for
  // it. If it runs it will update the request state.
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    static_cast<TestRenderFrameHost*>(frame_tree_node()->current_frame_host())
        ->SendBeforeUnloadACK(true);
  }

  if (!have_to_make_network_request)
    return;  // |request| is destructed by now.

  CHECK(request->state() == NavigationRequest::STARTED);

  if (!request->loader_for_testing()) {
    base::RunLoop loop;
    request->set_on_start_checks_complete_closure_for_testing(
        loop.QuitClosure());
    loop.Run();
  }

  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);

  // If a non-empty |redirect_url| was provided, simulate a server redirect.
  if (!redirect_url.is_empty())
    url_loader->SimulateServerRedirect(redirect_url);

  // Simulate the network stack commit.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  // TODO(carlosk): ideally with PlzNavigate it should be possible someday to
  // fully commit the navigation at this call to CallOnResponseStarted.
  url_loader->CallOnResponseStarted(response, MakeEmptyStream(), nullptr);
}

void TestRenderFrameHost::PrepareForCommitIfNecessary() {
  if (!IsBrowserSideNavigationEnabled() ||
      frame_tree_node()->navigation_request()) {
    PrepareForCommit();
  }
}

WebBluetoothServiceImpl*
TestRenderFrameHost::CreateWebBluetoothServiceForTesting() {
  WebBluetoothServiceImpl* service =
      RenderFrameHostImpl::CreateWebBluetoothService(
          blink::mojom::WebBluetoothServiceRequest());
  return service;
}

void TestRenderFrameHost::SimulateWillStartRequest(
    ui::PageTransition transition) {
  // PlzNavigate: NavigationHandle::WillStartRequest has already been called at
  // this point.
  if (!navigation_handle() || IsBrowserSideNavigationEnabled())
    return;
  navigation_handle()->CallWillStartRequestForTesting(
      false /* is_post */, Referrer(GURL(), blink::kWebReferrerPolicyDefault),
      true /* user_gesture */, transition, false /* is_external_protocol */);
}

}  // namespace content
