// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using base::FieldTrialList;
using chrome_common_net::DnsProbeResult;
using content::BrowserContext;
using content::BrowserThread;
using content::PageTransition;
using content::RenderViewHost;
using content::WebContents;
using content::WebContentsObserver;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::NetErrorTabHelper);

namespace chrome_browser_net {

namespace {

const char kDnsProbeFieldTrialName[] = "DnsProbe-Enable";
const char kDnsProbeFieldTrialEnableGroupName[] = "enable";

static NetErrorTabHelper::TestingState testing_state_ =
    NetErrorTabHelper::TESTING_DEFAULT;

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(int net_error) {
  return net_error == net::ERR_NAME_NOT_RESOLVED ||
         net_error == net::ERR_NAME_RESOLUTION_FAILED;
}

bool GetEnabledByTrial() {
  return (FieldTrialList::FindFullName(kDnsProbeFieldTrialName)
          == kDnsProbeFieldTrialEnableGroupName);
}

NetErrorTracker::FrameType GetFrameType(bool is_main_frame) {
  return is_main_frame ? NetErrorTracker::FRAME_MAIN
                       : NetErrorTracker::FRAME_SUB;
}

NetErrorTracker::PageType GetPageType(bool is_error_page) {
  return is_error_page ? NetErrorTracker::PAGE_ERROR
                       : NetErrorTracker::PAGE_NORMAL;
}

NetErrorTracker::ErrorType GetErrorType(int net_error) {
  return IsDnsError(net_error) ? NetErrorTracker::ERROR_DNS
                               : NetErrorTracker::ERROR_OTHER;
}

void OnDnsProbeFinishedOnIOThread(
    const base::Callback<void(DnsProbeResult)>& callback,
    DnsProbeResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DVLOG(1) << "DNS probe finished with result " << result;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, result));
}

void StartDnsProbeOnIOThread(
    const base::Callback<void(DnsProbeResult)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DVLOG(1) << "Starting DNS probe";

  DnsProbeService* probe_service =
      g_browser_process->io_thread()->globals()->dns_probe_service.get();

  probe_service->ProbeDns(base::Bind(&OnDnsProbeFinishedOnIOThread, callback));
}

}  // namespace

NetErrorTabHelper::~NetErrorTabHelper() {
}

// static
void NetErrorTabHelper::set_state_for_testing(TestingState state) {
  testing_state_ = state;
}

void NetErrorTabHelper::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  tracker_.OnStartProvisionalLoad(GetFrameType(is_main_frame),
                                  GetPageType(is_error_page));
}

void NetErrorTabHelper::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition transition_type,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  tracker_.OnCommitProvisionalLoad(GetFrameType(is_main_frame));
}

void NetErrorTabHelper::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  tracker_.OnFailProvisionalLoad(GetFrameType(is_main_frame),
                                 GetErrorType(error_code));
}

void NetErrorTabHelper::DidFinishLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  tracker_.OnFinishLoad(GetFrameType(is_main_frame));
}

NetErrorTabHelper::NetErrorTabHelper(WebContents* contents)
    : WebContentsObserver(contents),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(
          base::Bind(&NetErrorTabHelper::TrackerCallback,
                     weak_factory_.GetWeakPtr()))),
      dns_error_page_state_(NetErrorTracker::DNS_ERROR_PAGE_NONE),
      dns_probe_state_(DNS_PROBE_NONE),
      enabled_by_trial_(GetEnabledByTrial()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  InitializePref(contents);
}

void NetErrorTabHelper::TrackerCallback(
    NetErrorTracker::DnsErrorPageState state) {
  dns_error_page_state_ = state;

  MaybePostStartDnsProbeTask();
  MaybeSendInfo();
}

void NetErrorTabHelper::MaybePostStartDnsProbeTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (dns_error_page_state_ != NetErrorTracker::DNS_ERROR_PAGE_NONE &&
      dns_probe_state_ != DNS_PROBE_STARTED &&
      ProbesAllowed()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&StartDnsProbeOnIOThread,
                   base::Bind(&NetErrorTabHelper::OnDnsProbeFinished,
                              weak_factory_.GetWeakPtr())));
    dns_probe_state_ = DNS_PROBE_STARTED;
  }
}

void NetErrorTabHelper::OnDnsProbeFinished(DnsProbeResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(DNS_PROBE_STARTED, dns_probe_state_);

  dns_probe_result_ = result;
  dns_probe_state_ = DNS_PROBE_FINISHED;

  MaybeSendInfo();
}

void NetErrorTabHelper::MaybeSendInfo() {
  if (dns_error_page_state_ == NetErrorTracker::DNS_ERROR_PAGE_LOADED &&
      dns_probe_state_ == DNS_PROBE_FINISHED) {
    DVLOG(1) << "Sending result " << dns_probe_result_ << " to renderer";
    Send(new ChromeViewMsg_NetErrorInfo(routing_id(), dns_probe_result_));
    dns_probe_state_ = DNS_PROBE_NONE;
  }
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
  return enabled_by_trial_ && *resolve_errors_with_web_service_;
}

}  // namespace chrome_browser_net
