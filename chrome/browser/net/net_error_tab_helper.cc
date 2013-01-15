// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/prefs/pref_service.h"
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

void DnsProbeCallback(
    base::WeakPtr<NetErrorTabHelper> tab_helper,
    DnsProbeResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&NetErrorTabHelper::OnDnsProbeFinished,
                 tab_helper,
                 result));
}

void StartDnsProbe(
    base::WeakPtr<NetErrorTabHelper> tab_helper,
    IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DnsProbeService* probe_service =
      io_thread->globals()->dns_probe_service.get();
  probe_service->ProbeDns(base::Bind(&DnsProbeCallback, tab_helper));
}

}  // namespace

NetErrorTabHelper::NetErrorTabHelper(WebContents* contents)
    : WebContentsObserver(contents),
      dns_probe_running_(false),
      enabled_by_trial_(GetEnabledByTrial()),
      pref_initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  InitializePref(contents);
}

void NetErrorTabHelper::InitializePref(WebContents* contents) {
  // Unit tests don't pass a WebContents, so the tab helper has no way to get
  // to the preference.  pref_initialized_ will remain false, so ProbesAllowed
  // will return false without checking the pref.
  if (!contents)
    return;

  BrowserContext* browser_context = contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  resolve_errors_with_web_service_.Init(
      prefs::kAlternateErrorPagesEnabled,
      profile->GetPrefs());
  pref_initialized_ = true;
}

NetErrorTabHelper::~NetErrorTabHelper() {
}

void NetErrorTabHelper::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Consider running a DNS probe if a main frame load fails with a DNS error
  if (is_main_frame && IsDnsError(error_code))
    OnMainFrameDnsError();
}

void NetErrorTabHelper::OnMainFrameDnsError() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Don't start a probe if one is running already or we're not allowed to.
  if (dns_probe_running_ || !ProbesAllowed())
    return;

  PostStartDnsProbeTask();

  set_dns_probe_running(true);
}

void NetErrorTabHelper::OnDnsProbeFinished(DnsProbeResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(dns_probe_running_);

  // Notify renderer of DNS probe results.
  Send(new ChromeViewMsg_NetErrorInfo(routing_id(), result));

  set_dns_probe_running(false);
}

void NetErrorTabHelper::PostStartDnsProbeTask() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StartDnsProbe,
                 weak_factory_.GetWeakPtr(),
                 g_browser_process->io_thread()));
}

bool NetErrorTabHelper::ProbesAllowed() const {
  if (testing_state_ != TESTING_DEFAULT)
    return testing_state_ == TESTING_FORCE_ENABLED;

  // TODO(ttuttle): Disable on mobile?
  return enabled_by_trial_
         && pref_initialized_
         && *resolve_errors_with_web_service_;
}

// static
void NetErrorTabHelper::set_state_for_testing(TestingState state) {
  testing_state_ = state;
}

}  // namespace chrome_browser_net
