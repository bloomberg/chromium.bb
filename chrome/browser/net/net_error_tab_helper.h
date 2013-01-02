// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
#define CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/net/dns_probe_service.h"
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

  // content::WebContentsObserver implementation.
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;

  void OnDnsProbeFinished(DnsProbeService::Result result);

  static void set_state_for_testing(TestingState testing_state);

 protected:
  friend class content::WebContentsUserData<NetErrorTabHelper>;

  // |contents| is the WebContents of the tab this NetErrorTabHelper is
  // attached to.
  explicit NetErrorTabHelper(content::WebContents* contents);

  // Posts a task to the IO thread that will start a DNS probe.
  virtual void PostStartDnsProbeTask();

  // Checks if probes are allowed by enabled_for_testing and "use web service"
  // pref.
  virtual bool ProbesAllowed() const;

  bool dns_probe_running() { return dns_probe_running_; }
  void set_dns_probe_running(bool running) { dns_probe_running_ = running; }

 private:
  void InitializePref(content::WebContents* contents);

  void OnMainFrameDnsError();

  // Whether the tab helper has started a DNS probe that has not yet returned
  // a result.
  bool dns_probe_running_;
  // Whether we are enabled to run by the DnsProbe-Enable field trial.
  const bool enabled_by_trial_;
  // "Use a web service to resolve navigation errors" preference is required
  // to allow probes.
  BooleanPrefMember resolve_errors_with_web_service_;
  // Whether the above pref was initialized -- will be false in unit tests.
  bool pref_initialized_;
  base::WeakPtrFactory<NetErrorTabHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorTabHelper);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
