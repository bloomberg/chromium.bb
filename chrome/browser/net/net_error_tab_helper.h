// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
#define CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/net/net_error_tracker.h"
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

  virtual ~NetErrorTabHelper();

  static void set_state_for_testing(TestingState testing_state);

  // content::WebContentsObserver implementation.
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) OVERRIDE;

  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;

  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;

 private:
  friend class content::WebContentsUserData<NetErrorTabHelper>;

  enum DnsProbeState {
    DNS_PROBE_NONE,
    DNS_PROBE_STARTED,
    DNS_PROBE_FINISHED
  };

  // |contents| is the WebContents of the tab this NetErrorTabHelper is
  // attached to.
  explicit NetErrorTabHelper(content::WebContents* contents);

  void TrackerCallback(NetErrorTracker::DnsErrorPageState state);
  void MaybePostStartDnsProbeTask();
  void OnDnsProbeFinished(chrome_common_net::DnsProbeResult result);
  void MaybeSendInfo();

  void InitializePref(content::WebContents* contents);
  bool ProbesAllowed() const;

  base::WeakPtrFactory<NetErrorTabHelper> weak_factory_;

  NetErrorTracker tracker_;
  NetErrorTracker::DnsErrorPageState dns_error_page_state_;

  DnsProbeState dns_probe_state_;
  chrome_common_net::DnsProbeResult dns_probe_result_;

  // Whether we are enabled to run by the DnsProbe-Enable field trial.
  const bool enabled_by_trial_;
  // "Use a web service to resolve navigation errors" preference is required
  // to allow probes.
  BooleanPrefMember resolve_errors_with_web_service_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorTabHelper);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
