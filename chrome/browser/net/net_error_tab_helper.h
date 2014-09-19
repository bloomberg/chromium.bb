// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
#define CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/common/net/net_error_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace chrome_browser_net {

// A TabHelper that monitors loads for certain types of network errors and
// does interesting things with them.  Currently, starts DNS probes using the
// DnsProbeService whenever a page fails to load with a DNS-related error.
class NetErrorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NetErrorTabHelper> {
 public:
  enum TestingState {
    TESTING_DEFAULT,
    TESTING_FORCE_DISABLED,
    TESTING_FORCE_ENABLED
  };

  typedef base::Callback<void(chrome_common_net::DnsProbeStatus)>
      DnsProbeStatusSnoopCallback;

  virtual ~NetErrorTabHelper();

  static void set_state_for_testing(TestingState testing_state);

  // Sets a callback that will be called immediately after the helper sends
  // a NetErrorHelper IPC.  (Used by the DNS probe browser test to know when to
  // check the error page for updates, instead of polling.)
  void set_dns_probe_status_snoop_callback_for_testing(
      const DnsProbeStatusSnoopCallback& dns_probe_status_snoop_callback) {
    dns_probe_status_snoop_callback_ = dns_probe_status_snoop_callback;
  }

  // content::WebContentsObserver implementation.
  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;

  virtual void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) OVERRIDE;

  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) OVERRIDE;

  virtual void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description) OVERRIDE;

 protected:
  // |contents| is the WebContents of the tab this NetErrorTabHelper is
  // attached to.
  explicit NetErrorTabHelper(content::WebContents* contents);
  virtual void StartDnsProbe();
  virtual void SendInfo();
  void OnDnsProbeFinished(chrome_common_net::DnsProbeStatus result);

  chrome_common_net::DnsProbeStatus dns_probe_status() const {
    return dns_probe_status_;
  }

 private:
  friend class content::WebContentsUserData<NetErrorTabHelper>;

  void OnMainFrameDnsError();

  void InitializePref(content::WebContents* contents);
  bool ProbesAllowed() const;

  // True if the last provisional load that started was for an error page.
  bool is_error_page_;

  // True if the helper has seen a main frame page load fail with a DNS error,
  // but has not yet seen a new page commit successfully afterwards.
  bool dns_error_active_;

  // True if the helper has seen an error page commit while |dns_error_active_|
  // is true.  (This should never be true if |dns_error_active_| is false.)
  bool dns_error_page_committed_;

  // The status of a DNS probe that may or may not have started or finished.
  // Since the renderer can change out from under the helper (in cross-process
  // navigations), it re-sends the status whenever an error page commits.
  chrome_common_net::DnsProbeStatus dns_probe_status_;

  // Optional callback for browser test to snoop on outgoing NetErrorInfo IPCs.
  DnsProbeStatusSnoopCallback dns_probe_status_snoop_callback_;

  // "Use a web service to resolve navigation errors" preference is required
  // to allow probes.
  BooleanPrefMember resolve_errors_with_web_service_;

  base::WeakPtrFactory<NetErrorTabHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorTabHelper);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
