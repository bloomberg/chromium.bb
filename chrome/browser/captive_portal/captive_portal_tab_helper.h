// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TAB_HELPER_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace captive_portal {

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
// TODO(mmenke): Support redirects.  Needed for HSTS, which simulates redirects
//               at the network layer.  Also may reduce the number of
//               unnecessary captive portal checks on high latency connections.
//
// For the design doc, see:
// https://docs.google.com/document/d/1k-gP2sswzYNvryu9NcgN7q5XrsMlUdlUdoW9WRaEmfM/edit
class CaptivePortalTabHelper : public content::WebContentsObserver,
                               public content::NotificationObserver,
                               public base::NonThreadSafe {
 public:
  CaptivePortalTabHelper(Profile* profile,
                         content::WebContents* web_contents);
  virtual ~CaptivePortalTabHelper();

  // content::WebContentsObserver:
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
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

  virtual void DidStopLoading() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // A "Login Tab" is a tab that was originally at a captive portal login
  // page.  This is set to false when a captive portal is no longer detected.
  bool IsLoginTab() const;

 private:
  friend class CaptivePortalBrowserTest;
  friend class CaptivePortalTabHelperTest;

  // Called to indicate a tab is at, or is navigating to, the captive portal
  // login page.
  void SetIsLoginTab();

  // |this| takes ownership of |tab_reloader|.
  void SetTabReloaderForTest(CaptivePortalTabReloader* tab_reloader);

  CaptivePortalTabReloader* GetTabReloaderForTest();

  // Opens a login tab if the profile's active window doesn't have one already.
  void OpenLoginTab();

  // Neither of these will ever be NULL.
  scoped_ptr<CaptivePortalTabReloader> tab_reloader_;
  scoped_ptr<CaptivePortalLoginDetector> login_detector_;

  Profile* profile_;

  // If a provisional load has failed, and the tab is loading an error page, the
  // error code associated with the error page we're loading.
  // net::OK, otherwise.
  int pending_error_code_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalTabHelper);
};

}  // namespace captive_portal

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TAB_HELPER_H_
