// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::NetErrorTabHelper)

namespace chrome_browser_net {

namespace {

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(int net_error) {
  return net_error == net::ERR_NAME_NOT_RESOLVED ||
         net_error == net::ERR_NAME_RESOLUTION_FAILED;
}

void DnsProbeCallback(
    base::WeakPtr<NetErrorTabHelper> tab_helper,
    DnsProbeService::Result result) {
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

NetErrorTabHelper::NetErrorTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      dns_probe_running_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

NetErrorTabHelper::~NetErrorTabHelper() {
}

void NetErrorTabHelper::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Consider running a DNS probe if a main frame load fails with a DNS error
  if (is_main_frame && IsDnsError(error_code))
    OnMainFrameDnsError();
}

void NetErrorTabHelper::OnMainFrameDnsError() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Don't start a probe if one is running already or we're not allowed to.
  if (dns_probe_running_ || !DnsProbesAllowedByPref())
    return;

  PostStartDnsProbeTask();

  set_dns_probe_running(true);
}

void NetErrorTabHelper::OnDnsProbeFinished(
    DnsProbeService::Result result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(dns_probe_running_);

  // TODO(ttuttle): Notify renderer of probe results.

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

bool NetErrorTabHelper::DnsProbesAllowedByPref() const {
  // TODO(ttuttle): Actually check the pref.
  // TODO(ttuttle): Disable on browser_tests and maybe on mobile.
  return false;
}

}  // namespace chrome_browser_net
