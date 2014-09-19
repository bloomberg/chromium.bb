// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TAB_HELPER_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TAB_HELPER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/resource_type.h"

class GURL;
class Profile;

namespace content {
  class WebContents;
}

namespace net {
class SSLInfo;
}

class CaptivePortalLoginDetector;
class CaptivePortalTabReloader;

// Along with the classes it owns, responsible for detecting page loads broken
// by a captive portal, triggering captive portal checks on navigation events
// that may indicate a captive portal is present, or has been removed / logged
// in to, and taking any correcting actions.
//
// It acts as a WebContentsObserver for its CaptivePortalLoginDetector and
// CaptivePortalTabReloader.  It filters out non-main-frame resource loads, and
// treats the commit of an error page as a single event, rather than as 3
// (ProvisionalLoadFail, DidStartProvisionalLoad, DidCommit), which simplifies
// the CaptivePortalTabReloader.  It is also needed by CaptivePortalTabReloaders
// to inform the tab's CaptivePortalLoginDetector when the tab is at a captive
// portal's login page.
//
// The TabHelper assumes that a WebContents can only have one RenderViewHost
// with a provisional load at a time, and tracks only that navigation.  This
// assumption can be violated in rare cases, for example, a same-site
// navigation interrupted by a cross-process navigation started from the
// omnibox, may commit before it can be cancelled.  In these cases, this class
// may pass incorrect messages to the TabReloader, which will, at worst, result
// in not opening up a login tab until a second load fails or not automatically
// reloading a tab after logging in.
//
// For the design doc, see:
// https://docs.google.com/document/d/1k-gP2sswzYNvryu9NcgN7q5XrsMlUdlUdoW9WRaEmfM/edit
class CaptivePortalTabHelper
    : public content::WebContentsObserver,
      public content::NotificationObserver,
      public base::NonThreadSafe,
      public content::WebContentsUserData<CaptivePortalTabHelper> {
 public:
  virtual ~CaptivePortalTabHelper();

  // content::WebContentsObserver:
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;

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

  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // Called when a certificate interstitial error page is about to be shown.
  void OnSSLCertError(const net::SSLInfo& ssl_info);

  // A "Login Tab" is a tab that was originally at a captive portal login
  // page.  This is set to false when a captive portal is no longer detected.
  bool IsLoginTab() const;

 private:
  friend class CaptivePortalBrowserTest;
  friend class CaptivePortalTabHelperTest;

  friend class content::WebContentsUserData<CaptivePortalTabHelper>;
  explicit CaptivePortalTabHelper(content::WebContents* web_contents);

  // Called by Observe in response to the corresponding event.
  void OnRedirect(int child_id,
                  content::ResourceType resource_type,
                  const GURL& new_url);

  // Called by Observe in response to the corresponding event.
  void OnCaptivePortalResults(
      captive_portal::CaptivePortalResult previous_result,
      captive_portal::CaptivePortalResult result);

  void OnLoadAborted();

  // Called to indicate a tab is at, or is navigating to, the captive portal
  // login page.
  void SetIsLoginTab();

  // |this| takes ownership of |tab_reloader|.
  void SetTabReloaderForTest(CaptivePortalTabReloader* tab_reloader);

  const content::RenderViewHost* provisional_render_view_host() const {
    return provisional_render_view_host_;
  }

  CaptivePortalTabReloader* GetTabReloaderForTest();

  // Opens a login tab if the profile's active window doesn't have one already.
  void OpenLoginTab();

  Profile* profile_;

  // Neither of these will ever be NULL.
  scoped_ptr<CaptivePortalTabReloader> tab_reloader_;
  scoped_ptr<CaptivePortalLoginDetector> login_detector_;

  content::WebContents* web_contents_;

  // If a provisional load has failed, and the tab is loading an error page, the
  // error code associated with the error page we're loading.
  // net::OK, otherwise.
  int pending_error_code_;

  // The RenderViewHost with a provisional load, if any.  Can either be
  // the currently displayed RenderViewHost or a pending RenderViewHost for
  // cross-process navitations.  NULL when there's currently no provisional
  // load.
  content::RenderViewHost* provisional_render_view_host_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalTabHelper);
};

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TAB_HELPER_H_
