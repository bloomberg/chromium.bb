// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/net_errors.h"

using chrome_common_net::DnsProbeStatus;
using chrome_common_net::DnsProbeStatusToString;
using content::BrowserContext;
using content::BrowserThread;
using ui::PageTransition;
using content::RenderViewHost;
using content::WebContents;
using content::WebContentsObserver;

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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

void NetErrorTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!is_error_page_)
    return;

  // Only record reloads.
  if (reload_type != content::NavigationController::NO_RELOAD) {
    chrome_common_net::RecordEvent(
        chrome_common_net::NETWORK_ERROR_PAGE_BROWSER_INITIATED_RELOAD);
  }
}

void NetErrorTabHelper::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (render_frame_host->GetParent())
    return;

  is_error_page_ = is_error_page;
}

void NetErrorTabHelper::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    PageTransition transition_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
    const base::string16& error_description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (render_frame_host->GetParent())
    return;

  if (IsDnsError(error_code)) {
    dns_error_active_ = true;
    OnMainFrameDnsError();
  }
}

NetErrorTabHelper::NetErrorTabHelper(WebContents* contents)
    : WebContentsObserver(contents),
      is_error_page_(false),
      dns_error_active_(false),
      dns_error_page_committed_(false),
      dns_probe_status_(chrome_common_net::DNS_PROBE_POSSIBLE),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If this helper is under test, it won't have a WebContents.
  if (contents)
    InitializePref(contents);
}

void NetErrorTabHelper::OnMainFrameDnsError() {
  if (ProbesAllowed()) {
    // Don't start more than one probe at a time.
    if (dns_probe_status_ != chrome_common_net::DNS_PROBE_STARTED) {
      StartDnsProbe();
      dns_probe_status_ = chrome_common_net::DNS_PROBE_STARTED;
    }
  } else {
    dns_probe_status_ = chrome_common_net::DNS_PROBE_NOT_RUN;
  }
}

void NetErrorTabHelper::StartDnsProbe() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(dns_error_active_);
  DCHECK_NE(chrome_common_net::DNS_PROBE_STARTED, dns_probe_status_);

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome_common_net::DNS_PROBE_STARTED, dns_probe_status_);
  DCHECK(chrome_common_net::DnsProbeStatusIsFinished(result));

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
  DCHECK_NE(chrome_common_net::DNS_PROBE_POSSIBLE, dns_probe_status_);
  DCHECK(dns_error_page_committed_);

  DVLOG(1) << "Sending status " << DnsProbeStatusToString(dns_probe_status_);
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->Send(new ChromeViewMsg_NetErrorInfo(rfh->GetRoutingID(),
                                           dns_probe_status_));

  if (!dns_probe_status_snoop_callback_.is_null())
    dns_probe_status_snoop_callback_.Run(dns_probe_status_);
}

}  // namespace chrome_browser_net
