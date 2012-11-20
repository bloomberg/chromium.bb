// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "net/base/net_errors.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::NetErrorTabHelper)

namespace chrome_browser_net {

namespace {

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(int net_error) {
  return net_error == net::ERR_NAME_NOT_RESOLVED;
}

}  // namespace

NetErrorTabHelper::NetErrorTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      dns_probe_running_(false) {
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
  // If the error wasn't a DNS-related error, a DNS probe won't return any
  // useful information, so don't run one.
  //
  // If the load wasn't a main frame load, there's nowhere to put an error
  // page, so don't bother running a probe.  (The probes are intended to give
  // more useful troubleshooting ideas on DNS error pages.)
  //
  // If the tab helper has already started a probe, don't start another; it
  // will use the result from the first one.
  if (IsDnsError(error_code) && is_main_frame && !dns_probe_running_)
    StartDnsProbe();
}

void NetErrorTabHelper::StartDnsProbe() {
  DCHECK(!dns_probe_running_);

  // TODO(ttuttle): Start probe.

  set_dns_probe_running(true);
}

}  // namespace chrome_browser_net
