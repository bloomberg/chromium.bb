// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_content_renderer_client.h"

#include "android_webview/common/aw_resource.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/renderer/aw_key_systems.h"
#include "android_webview/renderer/aw_permission_client.h"
#include "android_webview/renderer/aw_render_frame_ext.h"
#include "android_webview/renderer/aw_render_view_ext.h"
#include "android_webview/renderer/print_render_frame_observer.h"
#include "android_webview/renderer/print_web_view_helper.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "url/gurl.h"

using content::RenderThread;

namespace android_webview {

AwContentRendererClient::AwContentRendererClient() {
}

AwContentRendererClient::~AwContentRendererClient() {
}

void AwContentRendererClient::RenderThreadStarted() {
  blink::WebString content_scheme(
      base::ASCIIToUTF16(android_webview::kContentScheme));
  blink::WebSecurityPolicy::registerURLSchemeAsLocal(content_scheme);

  blink::WebString aw_scheme(
      base::ASCIIToUTF16(android_webview::kAndroidWebViewVideoPosterScheme));
  blink::WebSecurityPolicy::registerURLSchemeAsSecure(aw_scheme);

  RenderThread* thread = RenderThread::Get();

  aw_render_process_observer_.reset(new AwRenderProcessObserver);
  thread->AddObserver(aw_render_process_observer_.get());

  visited_link_slave_.reset(new visitedlink::VisitedLinkSlave);
  thread->AddObserver(visited_link_slave_.get());
}

bool AwContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    content::DocumentState* document_state,
    int opener_id,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {

  // Only GETs can be overridden.
  if (!request.httpMethod().equals("GET"))
    return false;

  // Any navigation from loadUrl, and goBack/Forward are considered application-
  // initiated and hence will not yield a shouldOverrideUrlLoading() callback.
  // Webview classic does not consider reload application-initiated so we
  // continue the same behavior.
  // TODO(sgurun) is_content_initiated is normally false for cross-origin
  // navigations but since android_webview does not swap out renderers, this
  // works fine. This will stop working if android_webview starts swapping out
  // renderers on navigation.
  bool application_initiated =
      !document_state->navigation_state()->is_content_initiated()
      || type == blink::WebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return false;

  const GURL& gurl = request.url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (frame->parent() &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return false;

  // use NavigationInterception throttle to handle the call as that can
  // be deferred until after the java side has been constructed.
  if (opener_id != MSG_ROUTING_NONE) {
    return false;
  }

  bool ignore_navigation = false;
  base::string16 url =  request.url().string();

  int render_frame_id = render_frame->GetRoutingID();
  RenderThread::Get()->Send(new AwViewHostMsg_ShouldOverrideUrlLoading(
      render_frame_id, url, &ignore_navigation));
  return ignore_navigation;
}

void AwContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new AwPermissionClient(render_frame);
  new PrintRenderFrameObserver(render_frame);
  new AwRenderFrameExt(render_frame);

  // TODO(jam): when the frame tree moves into content and parent() works at
  // RenderFrame construction, simplify this by just checking parent().
  content::RenderFrame* parent_frame =
      render_frame->GetRenderView()->GetMainRenderFrame();
  if (parent_frame && parent_frame != render_frame) {
    // Avoid any race conditions from having the browser's UI thread tell the IO
    // thread that a subframe was created.
    RenderThread::Get()->Send(new AwViewHostMsg_SubFrameCreated(
        parent_frame->GetRoutingID(), render_frame->GetRoutingID()));
  }
}

void AwContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  AwRenderViewExt::RenderViewCreated(render_view);

  new printing::PrintWebViewHelper(render_view);
  // TODO(sgurun) do not create a password autofill agent (change
  // autofill agent to store a weakptr).
  autofill::PasswordAutofillAgent* password_autofill_agent =
      new autofill::PasswordAutofillAgent(render_view);
  new autofill::AutofillAgent(render_view, password_autofill_agent, NULL);
}

std::string AwContentRendererClient::GetDefaultEncoding() {
  return AwResource::GetDefaultTextEncoding();
}

bool AwContentRendererClient::HasErrorPage(int http_status_code,
                          std::string* error_domain) {
  return http_status_code >= 400;
}

void AwContentRendererClient::GetNavigationErrorStrings(
    content::RenderView* /* render_view */,
    blink::WebFrame* /* frame */,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html,
    base::string16* error_description) {
  if (error_html) {
    GURL error_url(failed_request.url());
    std::string err = base::UTF16ToUTF8(error.localizedDescription);
    std::string contents;
    if (err.empty()) {
      contents = AwResource::GetNoDomainPageContent();
    } else {
      contents = AwResource::GetLoadErrorPageContent();
      ReplaceSubstringsAfterOffset(&contents, 0, "%e", err);
    }

    ReplaceSubstringsAfterOffset(&contents, 0, "%s",
        net::EscapeForHTML(error_url.possibly_invalid_spec()));
    *error_html = contents;
  }
  if (error_description) {
    if (error.localizedDescription.isEmpty())
      *error_description = base::ASCIIToUTF16(net::ErrorToString(error.reason));
    else
      *error_description = error.localizedDescription;
  }
}

unsigned long long AwContentRendererClient::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool AwContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

void AwContentRendererClient::AddKeySystems(
    std::vector<content::KeySystemInfo>* key_systems) {
  AwAddKeySystems(key_systems);
}

}  // namespace android_webview
