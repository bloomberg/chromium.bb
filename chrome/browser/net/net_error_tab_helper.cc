// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/error_page/common/net_error_info.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/tab_android.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

using content::BrowserContext;
using content::BrowserThread;
using content::WebContents;
using content::WebContentsObserver;
using error_page::DnsProbeStatus;
using error_page::DnsProbeStatusToString;
using error_page::OfflinePageStatus;
using ui::PageTransition;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::NetErrorTabHelper);

namespace chrome_browser_net {

namespace {

static NetErrorTabHelper::TestingState testing_state_ =
    NetErrorTabHelper::TESTING_DEFAULT;

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(int net_error) {
  return net_error == net::ERR_NAME_NOT_RESOLVED ||
         net_error == net::ERR_NAME_RESOLUTION_FAILED;
}

void OnDnsProbeFinishedOnIOThread(
    const base::Callback<void(DnsProbeStatus)>& callback,
    DnsProbeStatus result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, result));
}

// Can only access g_browser_process->io_thread() from the browser thread,
// so have to pass it in to the callback instead of dereferencing it here.
void StartDnsProbeOnIOThread(
    const base::Callback<void(DnsProbeStatus)>& callback,
    IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DnsProbeService* probe_service =
      io_thread->globals()->dns_probe_service.get();

  probe_service->ProbeDns(base::Bind(&OnDnsProbeFinishedOnIOThread, callback));
}

}  // namespace

NetErrorTabHelper::~NetErrorTabHelper() {
}

// static
void NetErrorTabHelper::set_state_for_testing(TestingState state) {
  testing_state_ = state;
}

void NetErrorTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // Ignore subframe creation - only main frame error pages can link to the
  // platform's network diagnostics dialog.
  if (render_frame_host->GetParent())
    return;
  render_frame_host->Send(
      new ChromeViewMsg_SetCanShowNetworkDiagnosticsDialog(
          render_frame_host->GetRoutingID(),
          CanShowNetworkDiagnosticsDialog()));
}

void NetErrorTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!is_error_page_)
    return;

  // Only record reloads.
  if (reload_type != content::NavigationController::NO_RELOAD) {
    error_page::RecordEvent(
        error_page::NETWORK_ERROR_PAGE_BROWSER_INITIATED_RELOAD);
  }
}

void NetErrorTabHelper::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (render_frame_host->GetParent())
    return;

  is_error_page_ = is_error_page;

#if BUILDFLAG(ANDROID_JAVA_UI)
  SetOfflinePageInfo(render_frame_host, validated_url);
#endif  // BUILDFLAG(ANDROID_JAVA_UI)
}

void NetErrorTabHelper::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    PageTransition transition_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (render_frame_host->GetParent())
    return;

  // Resend status every time an error page commits; this is somewhat spammy,
  // but ensures that the status will make it to the real error page, even if
  // the link doctor loads a blank intermediate page or the tab switches
  // renderer processes.
  if (is_error_page_ && dns_error_active_) {
    dns_error_page_committed_ = true;
    DVLOG(1) << "Committed error page; resending status.";
    SendInfo();
  } else {
    dns_error_active_ = false;
    dns_error_page_committed_ = false;
  }
}

void NetErrorTabHelper::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (render_frame_host->GetParent())
    return;

  if (IsDnsError(error_code)) {
    dns_error_active_ = true;
    OnMainFrameDnsError();
  }
}

bool NetErrorTabHelper::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host != web_contents()->GetMainFrame())
    return false;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NetErrorTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RunNetworkDiagnostics,
                        RunNetworkDiagnostics)
#if BUILDFLAG(ANDROID_JAVA_UI)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ShowOfflinePages, ShowOfflinePages)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LoadOfflineCopy, LoadOfflineCopy)
#endif  // BUILDFLAG(ANDROID_JAVA_UI)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

NetErrorTabHelper::NetErrorTabHelper(WebContents* contents)
    : WebContentsObserver(contents),
      is_error_page_(false),
      dns_error_active_(false),
      dns_error_page_committed_(false),
      dns_probe_status_(error_page::DNS_PROBE_POSSIBLE),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If this helper is under test, it won't have a WebContents.
  if (contents)
    InitializePref(contents);
}

void NetErrorTabHelper::OnMainFrameDnsError() {
  if (ProbesAllowed()) {
    // Don't start more than one probe at a time.
    if (dns_probe_status_ != error_page::DNS_PROBE_STARTED) {
      StartDnsProbe();
      dns_probe_status_ = error_page::DNS_PROBE_STARTED;
    }
  } else {
    dns_probe_status_ = error_page::DNS_PROBE_NOT_RUN;
  }
}

void NetErrorTabHelper::StartDnsProbe() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(dns_error_active_);
  DCHECK_NE(error_page::DNS_PROBE_STARTED, dns_probe_status_);

  DVLOG(1) << "Starting DNS probe.";

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StartDnsProbeOnIOThread,
                 base::Bind(&NetErrorTabHelper::OnDnsProbeFinished,
                            weak_factory_.GetWeakPtr()),
                 g_browser_process->io_thread()));
}

void NetErrorTabHelper::OnDnsProbeFinished(DnsProbeStatus result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(error_page::DNS_PROBE_STARTED, dns_probe_status_);
  DCHECK(error_page::DnsProbeStatusIsFinished(result));

  DVLOG(1) << "Finished DNS probe with result "
           << DnsProbeStatusToString(result) << ".";

  dns_probe_status_ = result;

  if (dns_error_page_committed_)
    SendInfo();
}

void NetErrorTabHelper::InitializePref(WebContents* contents) {
  DCHECK(contents);

  BrowserContext* browser_context = contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  resolve_errors_with_web_service_.Init(
      prefs::kAlternateErrorPagesEnabled,
      profile->GetPrefs());
}

bool NetErrorTabHelper::ProbesAllowed() const {
  if (testing_state_ != TESTING_DEFAULT)
    return testing_state_ == TESTING_FORCE_ENABLED;

  // TODO(ttuttle): Disable on mobile?
  return *resolve_errors_with_web_service_;
}

void NetErrorTabHelper::SendInfo() {
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, dns_probe_status_);
  DCHECK(dns_error_page_committed_);

  DVLOG(1) << "Sending status " << DnsProbeStatusToString(dns_probe_status_);
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->Send(new ChromeViewMsg_NetErrorInfo(rfh->GetRoutingID(),
                                           dns_probe_status_));

  if (!dns_probe_status_snoop_callback_.is_null())
    dns_probe_status_snoop_callback_.Run(dns_probe_status_);
}

void NetErrorTabHelper::RunNetworkDiagnostics(const GURL& url) {
  // Only run diagnostics on HTTP or HTTPS URLs.  Shouldn't receive URLs with
  // any other schemes, but the renderer is not trusted.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;
  // Sanitize URL prior to running diagnostics on it.
  RunNetworkDiagnosticsHelper(url.GetOrigin().spec());
}

void NetErrorTabHelper::RunNetworkDiagnosticsHelper(
    const std::string& sanitized_url) {
  ShowNetworkDiagnosticsDialog(web_contents(), sanitized_url);
}

#if BUILDFLAG(ANDROID_JAVA_UI)
void NetErrorTabHelper::SetOfflinePageInfo(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  // Bails out if offline pages not supported.
  if (!offline_pages::IsOfflinePagesEnabled())
    return;

  offline_pages::OfflinePageModel* offline_page_model =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!offline_page_model)
    return;

  OfflinePageStatus status = OfflinePageStatus::NONE;
  if (offline_page_model->HasOfflinePages()) {
    status = offline_page_model->GetPageByOnlineURL(url)
                 ? OfflinePageStatus::HAS_OFFLINE_PAGE
                 : OfflinePageStatus::HAS_OTHER_OFFLINE_PAGES;
  }
  render_frame_host->Send(new ChromeViewMsg_SetOfflinePageInfo(
      render_frame_host->GetRoutingID(), status));
}

void NetErrorTabHelper::ShowOfflinePages() {
  // Makes sure that this is coming from an error page.
  if (!IsFromErrorPage())
    return;

  DCHECK(web_contents());
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  if (tab)
    tab->ShowOfflinePages();
}

void NetErrorTabHelper::LoadOfflineCopy(const GURL& url) {
  // Makes sure that this is coming from an error page.
  if (!IsFromErrorPage())
    return;

  GURL validated_url(url);
  if (!validated_url.is_valid())
    return;

  if (validated_url != web_contents()->GetLastCommittedURL())
    return;

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  if (tab)
    tab->LoadOfflineCopy(url);
}

bool NetErrorTabHelper::IsFromErrorPage() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return entry && (entry->GetPageType() == content::PAGE_TYPE_ERROR);
}

#endif  // BUILDFLAG(ANDROID_JAVA_UI)

}  // namespace chrome_browser_net
