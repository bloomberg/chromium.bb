// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_content_renderer_client.h"

#include <vector>

#include "android_webview/common/aw_resource.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/grit/aw_resources.h"
#include "android_webview/grit/aw_strings.h"
#include "android_webview/renderer/aw_content_settings_client.h"
#include "android_webview/renderer/aw_key_systems.h"
#include "android_webview/renderer/aw_message_port_client.h"
#include "android_webview/renderer/aw_print_web_view_helper_delegate.h"
#include "android_webview/renderer/aw_render_frame_ext.h"
#include "android_webview/renderer/aw_render_view_ext.h"
#include "android_webview/renderer/print_render_frame_observer.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/printing/renderer/print_web_view_helper.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::RenderThread;

namespace android_webview {

AwContentRendererClient::AwContentRendererClient() {}

AwContentRendererClient::~AwContentRendererClient() {}

void AwContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();
  aw_render_thread_observer_.reset(new AwRenderThreadObserver);
  thread->AddObserver(aw_render_thread_observer_.get());

  visited_link_slave_.reset(new visitedlink::VisitedLinkSlave);
  thread->AddObserver(visited_link_slave_.get());

  blink::WebString content_scheme(base::ASCIIToUTF16(url::kContentScheme));
  blink::WebSecurityPolicy::registerURLSchemeAsLocal(content_scheme);

  blink::WebString aw_scheme(
      base::ASCIIToUTF16(android_webview::kAndroidWebViewVideoPosterScheme));
  blink::WebSecurityPolicy::registerURLSchemeAsSecure(aw_scheme);
}

bool AwContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    bool is_content_initiated,
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
      !is_content_initiated || type == blink::WebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return false;

  bool is_main_frame = !frame->parent();
  const GURL& gurl = request.url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return false;

  // use NavigationInterception throttle to handle the call as that can
  // be deferred until after the java side has been constructed.
  if (opener_id != MSG_ROUTING_NONE) {
    return false;
  }

  bool ignore_navigation = false;
  base::string16 url = request.url().string();
  bool has_user_gesture = request.hasUserGesture();

  int render_frame_id = render_frame->GetRoutingID();
  RenderThread::Get()->Send(new AwViewHostMsg_ShouldOverrideUrlLoading(
      render_frame_id, url, has_user_gesture, is_redirect, is_main_frame,
      &ignore_navigation));
  return ignore_navigation;
}

void AwContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new AwContentSettingsClient(render_frame);
  new PrintRenderFrameObserver(render_frame);
  new AwRenderFrameExt(render_frame);
  new AwMessagePortClient(render_frame);

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

  // TODO(sgurun) do not create a password autofill agent (change
  // autofill agent to store a weakptr).
  autofill::PasswordAutofillAgent* password_autofill_agent =
      new autofill::PasswordAutofillAgent(render_frame);
  new autofill::AutofillAgent(render_frame, password_autofill_agent, NULL);
}

void AwContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  AwRenderViewExt::RenderViewCreated(render_view);

  new printing::PrintWebViewHelper(
      render_view, std::unique_ptr<printing::PrintWebViewHelper::Delegate>(
                       new AwPrintWebViewHelperDelegate()));
}

bool AwContentRendererClient::HasErrorPage(int http_status_code,
                          std::string* error_domain) {
  return http_status_code >= 400;
}

void AwContentRendererClient::GetNavigationErrorStrings(
    content::RenderFrame* /* render_frame */,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html,
    base::string16* error_description) {
  if (error_html) {
    std::string url =
        net::EscapeForHTML(GURL(failed_request.url()).possibly_invalid_spec());
    std::string err =
        base::UTF16ToUTF8(base::StringPiece16(error.localizedDescription));

    std::vector<std::string> replacements;
    replacements.push_back(
        l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_NOT_AVAILABLE));
    if (err.empty()) {
      replacements.push_back(l10n_util::GetStringFUTF8(
          IDS_AW_WEBPAGE_TEMPORARILY_DOWN, base::UTF8ToUTF16(url)));
      replacements.push_back(l10n_util::GetStringUTF8(
          IDS_AW_WEBPAGE_TEMPORARILY_DOWN_SUGGESTIONS));
    } else {
      replacements.push_back(l10n_util::GetStringFUTF8(
          IDS_AW_WEBPAGE_CAN_NOT_BE_LOADED, base::UTF8ToUTF16(url)));
      replacements.push_back(err);
    }
    if (base::i18n::IsRTL())
      replacements.push_back("direction: rtl;");
    else
      replacements.push_back("");
    *error_html = base::ReplaceStringPlaceholders(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_AW_LOAD_ERROR_HTML),
        replacements, nullptr);
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

void AwContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
  AwAddKeySystems(key_systems);
}

bool AwContentRendererClient::ShouldUseMediaPlayerForURL(const GURL& url) {
  // Android WebView needs to support codecs that Chrome does not, for these
  // cases we must force the usage of Android MediaPlayer instead of Chrome's
  // internal player.
  //
  // Note: Despite these extensions being forwarded for playback to MediaPlayer,
  // HTMLMediaElement.canPlayType() will return empty for these containers.
  // TODO(boliu): If this is important, extend media::MimeUtil for WebView.
  //
  // Format list mirrors:
  // http://developer.android.com/guide/appendix/media-formats.html
  static const char* kMediaPlayerExtensions[] = {
      ".3gp",  ".ts",    ".flac", ".mid", ".xmf",
      ".mxmf", ".rtttl", ".rtx",  ".ota", ".imy"};
  for (const auto& extension : kMediaPlayerExtensions) {
    if (base::EndsWith(url.path(), extension,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

}  // namespace android_webview
