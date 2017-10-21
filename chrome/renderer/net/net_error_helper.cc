// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/grit/components_resources.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/common/associated_interface_registry.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/resource_fetcher.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/WebKit/public/platform/WebCachePolicy.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDocumentLoader.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/gurl.h"

using base::JSONWriter;
using content::DocumentState;
using content::RenderFrame;
using content::RenderFrameObserver;
using content::RenderThread;
using content::kUnreachableWebDataURL;
using error_page::DnsProbeStatus;
using error_page::DnsProbeStatusToString;
using error_page::ErrorPageParams;
using error_page::LocalizedError;

namespace {

// Number of seconds to wait for the navigation correction service to return
// suggestions.  If it takes too long, just use the local error page.
const int kNavigationCorrectionFetchTimeoutSec = 3;

NetErrorHelperCore::PageType GetLoadingPageType(
    blink::WebDocumentLoader* document_loader) {
  GURL url = document_loader->GetRequest().Url();
  if (!url.is_valid() || url.spec() != kUnreachableWebDataURL)
    return NetErrorHelperCore::NON_ERROR_PAGE;
  return NetErrorHelperCore::ERROR_PAGE;
}

NetErrorHelperCore::FrameType GetFrameType(RenderFrame* render_frame) {
  if (render_frame->IsMainFrame())
    return NetErrorHelperCore::MAIN_FRAME;
  return NetErrorHelperCore::SUB_FRAME;
}

const net::NetworkTrafficAnnotationTag& GetNetworkTrafficAnnotationTag() {
  static const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag =
      net::DefineNetworkTrafficAnnotation("net_error_helper", R"(
    semantics {
      sender: "NetErrorHelper"
      description:
        "Chrome asks Link Doctor service when a navigating page returns an "
        "error to investigate details about what is wrong."
      trigger:
        "When Chrome navigates to a page, and the page returns an error."
      data:
        "Failed page information including the URL will be sent to the service."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "You can enable or disable this feature via 'Use a web service to help "
        "resolve navigation errors' in Chrome's settings under Advanced. The "
        "feature is enabled by default."
      chrome_policy {
        AlternateErrorPagesEnabled {
          policy_options {mode: MANDATORY}
          AlternateErrorPagesEnabled: false
        }
      }
    })");
  return network_traffic_annotation_tag;
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<NetErrorHelper>(render_frame),
      weak_controller_delegate_factory_(this) {
  RenderThread::Get()->AddObserver(this);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool auto_reload_enabled =
      command_line->HasSwitch(switches::kEnableOfflineAutoReload);
  bool auto_reload_visible_only =
      command_line->HasSwitch(switches::kEnableOfflineAutoReloadVisibleOnly);
  // TODO(mmenke): Consider only creating a NetErrorHelperCore for main frames.
  // subframes don't need any of the NetErrorHelperCore's extra logic.
  core_.reset(new NetErrorHelperCore(this,
                                     auto_reload_enabled,
                                     auto_reload_visible_only,
                                     !render_frame->IsHidden()));

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::Bind(&NetErrorHelper::OnNetworkDiagnosticsClientRequest,
                 base::Unretained(this)));
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
      &NetErrorHelper::OnNavigationCorrectorRequest, base::Unretained(this)));
}

NetErrorHelper::~NetErrorHelper() {
  RenderThread::Get()->RemoveObserver(this);
}

void NetErrorHelper::ButtonPressed(NetErrorHelperCore::Button button) {
  core_->ExecuteButtonPress(button);
}

void NetErrorHelper::TrackClick(int tracking_id) {
  core_->TrackClick(tracking_id);
}

void NetErrorHelper::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader) {
  core_->OnStartLoad(GetFrameType(render_frame()),
                     GetLoadingPageType(document_loader));
}

void NetErrorHelper::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_document_navigation) {
  // If this is a "same-document" navigation, it's not a real navigation.  There
  // wasn't a start event for it, either, so just ignore it.
  if (is_same_document_navigation)
    return;

  // Invalidate weak pointers from old error page controllers. If loading a new
  // error page, the controller has not yet been attached, so this won't affect
  // it.
  weak_controller_delegate_factory_.InvalidateWeakPtrs();

  core_->OnCommitLoad(GetFrameType(render_frame()),
                      render_frame()->GetWebFrame()->GetDocument().Url());
}

void NetErrorHelper::DidFinishLoad() {
  core_->OnFinishLoad(GetFrameType(render_frame()));
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

void NetErrorHelper::OnDestruct() {
  delete this;
}

void NetErrorHelper::NetworkStateChanged(bool enabled) {
  core_->NetworkStateChanged(enabled);
}

void NetErrorHelper::GetErrorHTML(const error_page::Error& error,
                                  bool is_failed_post,
                                  bool is_ignoring_cache,
                                  std::string* error_html) {
  core_->GetErrorHTML(GetFrameType(render_frame()), error, is_failed_post,
                      is_ignoring_cache, error_html);
}

bool NetErrorHelper::ShouldSuppressErrorPage(const GURL& url) {
  return core_->ShouldSuppressErrorPage(GetFrameType(render_frame()), url);
}

chrome::mojom::NetworkDiagnostics*
NetErrorHelper::GetRemoteNetworkDiagnostics() {
  if (!remote_network_diagnostics_) {
    render_frame()->GetRemoteAssociatedInterfaces()
        ->GetInterface(&remote_network_diagnostics_);
  }
  return remote_network_diagnostics_.get();
}

void NetErrorHelper::GenerateLocalizedErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    bool can_show_network_diagnostics_dialog,
    std::unique_ptr<ErrorPageParams> params,
    bool* reload_button_shown,
    bool* show_saved_copy_button_shown,
    bool* show_cached_copy_button_shown,
    bool* download_button_shown,
    std::string* error_html) const {
  error_html->clear();

  int resource_id = IDR_NET_ERROR_HTML;
  const base::StringPiece template_html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (template_html.empty()) {
    NOTREACHED() << "unable to load template.";
  } else {
    base::DictionaryValue error_strings;
    LocalizedError::GetStrings(
        error.reason(), error.domain(), error.url(), is_failed_post,
        error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
        ChromeRenderThreadObserver::is_incognito_process(),
        RenderThread::Get()->GetLocale(), std::move(params), &error_strings);
    *reload_button_shown = error_strings.Get("reloadButton", nullptr);
    *show_saved_copy_button_shown =
        error_strings.Get("showSavedCopyButton", nullptr);
    *show_cached_copy_button_shown =
        error_strings.Get("cacheButton", nullptr);
    *download_button_shown =
        error_strings.Get("downloadButton", nullptr);
    // "t" is the id of the template's root node.
    *error_html = webui::GetTemplatesHtml(template_html, &error_strings, "t");
  }
}

void NetErrorHelper::LoadErrorPage(const std::string& html,
                                   const GURL& failed_url) {
  render_frame()->GetWebFrame()->LoadHTMLString(
      html, GURL(kUnreachableWebDataURL), failed_url, true);
}

void NetErrorHelper::EnablePageHelperFunctions() {
  NetErrorPageController::Install(
      render_frame(), weak_controller_delegate_factory_.GetWeakPtr());
}

void NetErrorHelper::UpdateErrorPage(const error_page::Error& error,
                                     bool is_failed_post,
                                     bool can_show_network_diagnostics_dialog) {
  base::DictionaryValue error_strings;
  LocalizedError::GetStrings(
      error.reason(), error.domain(), error.url(), is_failed_post,
      error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
      ChromeRenderThreadObserver::is_incognito_process(),
      RenderThread::Get()->GetLocale(), std::unique_ptr<ErrorPageParams>(),
      &error_strings);

  std::string json;
  JSONWriter::Write(error_strings, &json);

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

  correction_fetcher_ =
      content::ResourceFetcher::Create(navigation_correction_url);
  correction_fetcher_->SetMethod("POST");
  correction_fetcher_->SetBody(navigation_correction_request_body);
  correction_fetcher_->SetHeader("Content-Type", "application/json");

  correction_fetcher_->Start(
      render_frame()->GetWebFrame(),
      blink::WebURLRequest::kRequestContextInternal,
      render_frame()
          ->GetDefaultURLLoaderFactoryGetter()
          ->GetNetworkLoaderFactory(),
      GetNetworkTrafficAnnotationTag(),
      base::BindOnce(&NetErrorHelper::OnNavigationCorrectionsFetched,
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
  // If there's already a pending tracking request, this will cancel it.
  tracking_fetcher_ = content::ResourceFetcher::Create(tracking_url);
  tracking_fetcher_->SetMethod("POST");
  tracking_fetcher_->SetBody(tracking_request_body);
  tracking_fetcher_->SetHeader("Content-Type", "application/json");

  tracking_fetcher_->Start(
      render_frame()->GetWebFrame(),
      blink::WebURLRequest::kRequestContextInternal,
      render_frame()
          ->GetDefaultURLLoaderFactoryGetter()
          ->GetNetworkLoaderFactory(),
      GetNetworkTrafficAnnotationTag(),
      base::BindOnce(&NetErrorHelper::OnTrackingRequestComplete,
                     base::Unretained(this)));
}

void NetErrorHelper::ReloadPage(bool bypass_cache) {
  render_frame()->GetWebFrame()->Reload(
      bypass_cache ? blink::WebFrameLoadType::kReloadBypassingCache
                   : blink::WebFrameLoadType::kReload);
}

void NetErrorHelper::LoadPageFromCache(const GURL& page_url) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK_NE("POST",
            web_frame->GetDocumentLoader()->GetRequest().HttpMethod().Ascii());

  blink::WebURLRequest request(page_url);
  request.SetCachePolicy(blink::WebCachePolicy::kReturnCacheDataDontLoad);
  web_frame->LoadRequest(request);
}

void NetErrorHelper::DiagnoseError(const GURL& page_url) {
  GetRemoteNetworkDiagnostics()->RunNetworkDiagnostics(page_url);
}

void NetErrorHelper::DownloadPageLater() {
#if defined(OS_ANDROID)
  render_frame()->Send(new ChromeViewHostMsg_DownloadPageLater(
      render_frame()->GetRoutingID()));
#endif  // defined(OS_ANDROID)
}

void NetErrorHelper::SetIsShowingDownloadButton(bool show) {
#if defined(OS_ANDROID)
  render_frame()->Send(
      new ChromeViewHostMsg_SetIsShowingDownloadButtonInErrorPage(
          render_frame()->GetRoutingID(), show));
#endif  // defined(OS_ANDROID)
}

void NetErrorHelper::DNSProbeStatus(int32_t status_num) {
  DCHECK(status_num >= 0 && status_num < error_page::DNS_PROBE_MAX);

  DVLOG(1) << "Received status " << DnsProbeStatusToString(status_num);

  core_->OnNetErrorInfo(static_cast<DnsProbeStatus>(status_num));
}

void NetErrorHelper::SetNavigationCorrectionInfo(
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
  std::unique_ptr<content::ResourceFetcher> fetcher(
      correction_fetcher_.release());
  bool success = (!response.IsNull() && response.HttpStatusCode() == 200);
  core_->OnNavigationCorrectionsFetched(success ? data : "",
                                        base::i18n::IsRTL());
}

void NetErrorHelper::OnTrackingRequestComplete(
    const blink::WebURLResponse& response,
    const std::string& data) {
  tracking_fetcher_.reset();
}

void NetErrorHelper::OnNetworkDiagnosticsClientRequest(
    chrome::mojom::NetworkDiagnosticsClientAssociatedRequest request) {
  network_diagnostics_client_bindings_.AddBinding(this, std::move(request));
}

void NetErrorHelper::OnNavigationCorrectorRequest(
    chrome::mojom::NavigationCorrectorAssociatedRequest request) {
  navigation_corrector_bindings_.AddBinding(this, std::move(request));
}

void NetErrorHelper::SetCanShowNetworkDiagnosticsDialog(bool can_show) {
  core_->OnSetCanShowNetworkDiagnosticsDialog(can_show);
}
