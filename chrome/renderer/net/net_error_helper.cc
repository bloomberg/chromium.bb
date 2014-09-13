// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <string>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/net/net_error_page_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/resource_fetcher.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/gurl.h"

using base::JSONWriter;
using chrome_common_net::DnsProbeStatus;
using chrome_common_net::DnsProbeStatusToString;
using content::DocumentState;
using content::RenderFrame;
using content::RenderFrameObserver;
using content::RenderThread;
using content::kUnreachableWebDataURL;

namespace {

// Number of seconds to wait for the navigation correction service to return
// suggestions.  If it takes too long, just use the local error page.
static const int kNavigationCorrectionFetchTimeoutSec = 3;

NetErrorHelperCore::PageType GetLoadingPageType(const blink::WebFrame* frame) {
  GURL url = frame->provisionalDataSource()->request().url();
  if (!url.is_valid() || url.spec() != kUnreachableWebDataURL)
    return NetErrorHelperCore::NON_ERROR_PAGE;
  return NetErrorHelperCore::ERROR_PAGE;
}

NetErrorHelperCore::FrameType GetFrameType(const blink::WebFrame* frame) {
  if (!frame->parent())
    return NetErrorHelperCore::MAIN_FRAME;
  return NetErrorHelperCore::SUB_FRAME;
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<NetErrorHelper>(render_frame) {
  RenderThread::Get()->AddObserver(this);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool auto_reload_enabled =
      command_line->HasSwitch(switches::kEnableOfflineAutoReload);
  bool auto_reload_visible_only =
      command_line->HasSwitch(switches::kEnableOfflineAutoReloadVisibleOnly);
  core_.reset(new NetErrorHelperCore(this,
                                     auto_reload_enabled,
                                     auto_reload_visible_only,
                                     !render_frame->IsHidden()));
}

NetErrorHelper::~NetErrorHelper() {
  RenderThread::Get()->RemoveObserver(this);
}

void NetErrorHelper::ReloadButtonPressed() {
  core_->ExecuteButtonPress(NetErrorHelperCore::RELOAD_BUTTON);
}

void NetErrorHelper::LoadStaleButtonPressed() {
  core_->ExecuteButtonPress(NetErrorHelperCore::LOAD_STALE_BUTTON);
}

void NetErrorHelper::MoreButtonPressed() {
  core_->ExecuteButtonPress(NetErrorHelperCore::MORE_BUTTON);
}

void NetErrorHelper::DidStartProvisionalLoad() {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  core_->OnStartLoad(GetFrameType(frame), GetLoadingPageType(frame));
}

void NetErrorHelper::DidCommitProvisionalLoad(bool is_new_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  core_->OnCommitLoad(GetFrameType(frame), frame->document().url());
}

void NetErrorHelper::DidFinishLoad() {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  core_->OnFinishLoad(GetFrameType(frame));
}

void NetErrorHelper::OnStop() {
  core_->OnStop();
}

void NetErrorHelper::WasShown() {
  core_->OnWasShown();
}

void NetErrorHelper::WasHidden() {
  core_->OnWasHidden();
}

bool NetErrorHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(NetErrorHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_NetErrorInfo, OnNetErrorInfo)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetNavigationCorrectionInfo,
                        OnSetNavigationCorrectionInfo);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NetErrorHelper::NetworkStateChanged(bool enabled) {
  core_->NetworkStateChanged(enabled);
}

void NetErrorHelper::GetErrorHTML(
    blink::WebFrame* frame,
    const blink::WebURLError& error,
    bool is_failed_post,
    std::string* error_html) {
  core_->GetErrorHTML(GetFrameType(frame), error, is_failed_post, error_html);
}

bool NetErrorHelper::ShouldSuppressErrorPage(blink::WebFrame* frame,
                                             const GURL& url) {
  return core_->ShouldSuppressErrorPage(GetFrameType(frame), url);
}

void NetErrorHelper::TrackClick(int tracking_id) {
  core_->TrackClick(tracking_id);
}

void NetErrorHelper::GenerateLocalizedErrorPage(
    const blink::WebURLError& error,
    bool is_failed_post,
    scoped_ptr<LocalizedError::ErrorPageParams> params,
    bool* reload_button_shown,
    bool* load_stale_button_shown,
    std::string* error_html) const {
  error_html->clear();

  int resource_id = IDR_NET_ERROR_HTML;
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (template_html.empty()) {
    NOTREACHED() << "unable to load template.";
  } else {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    bool load_stale_cache_enabled =
        command_line->HasSwitch(switches::kEnableOfflineLoadStaleCache);

    base::DictionaryValue error_strings;
    LocalizedError::GetStrings(error.reason, error.domain.utf8(),
                               error.unreachableURL, is_failed_post,
                               (load_stale_cache_enabled &&
                                error.staleCopyInCache && !is_failed_post),
                               RenderThread::Get()->GetLocale(),
                               render_frame()->GetRenderView()->
                                   GetAcceptLanguages(),
                               params.Pass(), &error_strings);
    *reload_button_shown = error_strings.Get("reloadButton", NULL);
    *load_stale_button_shown = error_strings.Get("staleLoadButton", NULL);

    // "t" is the id of the template's root node.
    *error_html = webui::GetTemplatesHtml(template_html, &error_strings, "t");
  }
}

void NetErrorHelper::LoadErrorPageInMainFrame(const std::string& html,
                                              const GURL& failed_url) {
  blink::WebView* web_view = render_frame()->GetRenderView()->GetWebView();
  if (!web_view)
    return;
  blink::WebFrame* frame = web_view->mainFrame();
  frame->loadHTMLString(html, GURL(kUnreachableWebDataURL), failed_url, true);
}

void NetErrorHelper::EnablePageHelperFunctions() {
  NetErrorPageController::Install(render_frame());
}

void NetErrorHelper::UpdateErrorPage(const blink::WebURLError& error,
                                     bool is_failed_post) {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    bool load_stale_cache_enabled =
        command_line->HasSwitch(switches::kEnableOfflineLoadStaleCache);

  base::DictionaryValue error_strings;
  LocalizedError::GetStrings(error.reason,
                             error.domain.utf8(),
                             error.unreachableURL,
                             is_failed_post,
                             (load_stale_cache_enabled &&
                              error.staleCopyInCache && !is_failed_post),
                             RenderThread::Get()->GetLocale(),
                             render_frame()->GetRenderView()->
                                 GetAcceptLanguages(),
                             scoped_ptr<LocalizedError::ErrorPageParams>(),
                             &error_strings);

  std::string json;
  JSONWriter::Write(&error_strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  base::string16 js16;
  if (!base::UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    NOTREACHED();
    return;
  }

  render_frame()->ExecuteJavaScript(js16);
}

void NetErrorHelper::FetchNavigationCorrections(
    const GURL& navigation_correction_url,
    const std::string& navigation_correction_request_body) {
  DCHECK(!correction_fetcher_.get());

  blink::WebView* web_view = render_frame()->GetRenderView()->GetWebView();
  if (!web_view)
    return;
  blink::WebFrame* frame = web_view->mainFrame();

  correction_fetcher_.reset(
      content::ResourceFetcher::Create(navigation_correction_url));
  correction_fetcher_->SetMethod("POST");
  correction_fetcher_->SetBody(navigation_correction_request_body);
  correction_fetcher_->SetHeader("Content-Type", "application/json");

  correction_fetcher_->Start(
      frame,
      blink::WebURLRequest::RequestContextInternal,
      blink::WebURLRequest::FrameTypeTopLevel,
      content::ResourceFetcher::PLATFORM_LOADER,
      base::Bind(&NetErrorHelper::OnNavigationCorrectionsFetched,
                 base::Unretained(this)));

  correction_fetcher_->SetTimeout(
      base::TimeDelta::FromSeconds(kNavigationCorrectionFetchTimeoutSec));
}

void NetErrorHelper::CancelFetchNavigationCorrections() {
  correction_fetcher_.reset();
}

void NetErrorHelper::SendTrackingRequest(
    const GURL& tracking_url,
    const std::string& tracking_request_body) {
  blink::WebView* web_view = render_frame()->GetRenderView()->GetWebView();
  if (!web_view)
    return;
  blink::WebFrame* frame = web_view->mainFrame();

  // If there's already a pending tracking request, this will cancel it.
  tracking_fetcher_.reset(content::ResourceFetcher::Create(tracking_url));
  tracking_fetcher_->SetMethod("POST");
  tracking_fetcher_->SetBody(tracking_request_body);
  tracking_fetcher_->SetHeader("Content-Type", "application/json");

  tracking_fetcher_->Start(
      frame,
      blink::WebURLRequest::RequestContextInternal,
      blink::WebURLRequest::FrameTypeTopLevel,
      content::ResourceFetcher::PLATFORM_LOADER,
      base::Bind(&NetErrorHelper::OnTrackingRequestComplete,
                 base::Unretained(this)));
}

void NetErrorHelper::ReloadPage() {
  render_frame()->GetWebFrame()->reload(false);
}

void NetErrorHelper::LoadPageFromCache(const GURL& page_url) {
  blink::WebFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK(!EqualsASCII(web_frame->dataSource()->request().httpMethod(), "POST"));

  blink::WebURLRequest request(page_url);
  request.setCachePolicy(blink::WebURLRequest::ReturnCacheDataDontLoad);

  web_frame->loadRequest(request);
}

void NetErrorHelper::OnNetErrorInfo(int status_num) {
  DCHECK(status_num >= 0 && status_num < chrome_common_net::DNS_PROBE_MAX);

  DVLOG(1) << "Received status " << DnsProbeStatusToString(status_num);

  core_->OnNetErrorInfo(static_cast<DnsProbeStatus>(status_num));
}

void NetErrorHelper::OnSetNavigationCorrectionInfo(
    const GURL& navigation_correction_url,
    const std::string& language,
    const std::string& country_code,
    const std::string& api_key,
    const GURL& search_url) {
  core_->OnSetNavigationCorrectionInfo(navigation_correction_url, language,
                                      country_code, api_key, search_url);
}

void NetErrorHelper::OnNavigationCorrectionsFetched(
    const blink::WebURLResponse& response,
    const std::string& data) {
  // The fetcher may only be deleted after |data| is passed to |core_|.  Move
  // it to a temporary to prevent any potential re-entrancy issues.
  scoped_ptr<content::ResourceFetcher> fetcher(
      correction_fetcher_.release());
  bool success = (!response.isNull() && response.httpStatusCode() == 200);
  core_->OnNavigationCorrectionsFetched(
      success ? data : "",
      render_frame()->GetRenderView()->GetAcceptLanguages(),
      base::i18n::IsRTL());
}

void NetErrorHelper::OnTrackingRequestComplete(
    const blink::WebURLResponse& response,
    const std::string& data) {
  tracking_fetcher_.reset();
}
