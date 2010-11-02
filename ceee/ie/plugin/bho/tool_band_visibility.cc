// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ceee/ie/plugin/bho/tool_band_visibility.h"

#include "base/logging.h"
#include "ceee/ie/common/ie_tab_interfaces.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/toolband/tool_band.h"

// The ToolBandVisibility class allows us to recover from a number of
// features of IE that can cause our toolband to become invisible
// unexpectedly.

// A brief discussion of toolband visibility in IE.
//
// See MS knowledge base article Q219427 for more detail.
//
// IE does some interesting tricks to cache toolband layout information.  One
// side effect of this is that sometimes it can get confused about whether a
// toolband should be shown or not.
//
// In theory all that is needed to recover from this is a call to
// IWebBrowser2::ShowBrowserBar.  Unfortunately, there are some
// gotchas.
//    It's not easy to tell when IE has refused to display your
//      toolband.  The only way to be sure is to either call
//      ShowBrowserBar on every startup or wait for some reasonable
//      time to see if IE showed the toolband and then kick it if it
//      didn't.
//    In IE6, a single call to ShowBrowserBar is often not enough to
//      unhide a hidden toolband.  It's sometimes necessary to call
//      ShowBrowserBar THREE times (show, then hide, then show) to get
//      things into a sane state.
// TODO(cindylau@chromium.org): In IE6, just calling ShowBrowserBar
//      will usually cause IE to scrunch our toolband up at the end of
//      a line instead of giving it its own line. We can combat this
//      by requesting to be shown on our own line, but if we do that
//      in all cases we'll anger users who WANT our toolband to share
//      a line with others.
//    Some other toolbands (notably SnagIt versions 6 and 7) will
//      cause toolbands to get hidden when opening a new tab in IE7.
//      Visibility must be checked on every new tab and window to be
//      sure.
//    Calls to ShowBrowserBar are slow and we should avoid them
//      whenever possible.
//    Calls to ShowBrowserBar should be made from the same UI thread
//      responsible for the browser object we use to unhide the
//      toolband.  Failure to do this can cause the toolband to
//      believe it belongs to a different thread than it does.
// TODO(cindylau@chromium.org): IE tracks layout information in the
//      registry.  When toolbands are added or removed the installing
//      toolband MUST clear the registry (badness, including possible
//      crashes in IE will result if not).  When this layout
//      information is cleared all third party toolbands are hidden.

// This code attempts to address all of these issues.
// The core is the VisibilityUtil class.
// When VisibilityUtil::CheckVisibility is called it checks for common
// indicators that a toolband is hidden.  If it sees a smoking gun
// it unhides the toolband.  Otherwise it creates a notification window and
// a timer.  If a toolband doesn't report itself as active for a particular
// browser window before the timer fires it unhides the toolband.

namespace {
const int kVisibilityTimerId = 1;
const DWORD kVisibilityCheckDelay = 2000;  // 2 seconds.
}  // anonymous namespace

std::set<IUnknown*> ToolBandVisibility::visibility_set_;
CComAutoCriticalSection ToolBandVisibility::visibility_set_crit_;

ToolBandVisibility::ToolBandVisibility()
    : web_browser_(NULL) {
}

ToolBandVisibility::~ToolBandVisibility() {
  DCHECK(m_hWnd == NULL);
}

void ToolBandVisibility::ReportToolBandVisible(IWebBrowser2* web_browser) {
  DCHECK(web_browser);
  CComQIPtr<IUnknown, &IID_IUnknown> browser_identity(web_browser);
  DCHECK(browser_identity != NULL);
  if (browser_identity == NULL)
    return;
  CComCritSecLock<CComAutoCriticalSection> lock(visibility_set_crit_);
  visibility_set_.insert(browser_identity);
}

bool ToolBandVisibility::IsToolBandVisible(IWebBrowser2* web_browser) {
  DCHECK(web_browser);
  CComQIPtr<IUnknown, &IID_IUnknown> browser_identity(web_browser);
  DCHECK(browser_identity != NULL);
  if (browser_identity == NULL)
    return false;
  CComCritSecLock<CComAutoCriticalSection> lock(visibility_set_crit_);
  return visibility_set_.count(browser_identity) != 0;
}

void ToolBandVisibility::ClearCachedVisibility(IWebBrowser2* web_browser) {
  CComCritSecLock<CComAutoCriticalSection> lock(visibility_set_crit_);
  if (web_browser) {
    CComQIPtr<IUnknown, &IID_IUnknown> browser_identity(web_browser);
    DCHECK(browser_identity != NULL);
    if (browser_identity == NULL)
      return;
    visibility_set_.erase(browser_identity);
  } else {
    visibility_set_.clear();
  }
}

void ToolBandVisibility::CheckToolBandVisibility(IWebBrowser2* web_browser) {
  DCHECK(web_browser);
  web_browser_ = web_browser;

  if (!ceee_module_util::GetOptionToolbandIsHidden() &&
      CreateNotificationWindow()) {
    SetWindowTimer(kVisibilityTimerId, kVisibilityCheckDelay);
  }
}

void ToolBandVisibility::TearDown() {
  if (web_browser_ != NULL) {
    ClearCachedVisibility(web_browser_);
  }
  if (m_hWnd != NULL) {
    CloseNotificationWindow();
  }
}

bool ToolBandVisibility::CreateNotificationWindow() {
  return Create(HWND_MESSAGE) != NULL;
}

void ToolBandVisibility::CloseNotificationWindow() {
  DestroyWindow();
}

void ToolBandVisibility::SetWindowTimer(UINT timer_id, UINT delay) {
  SetTimer(timer_id, delay, NULL);
}

void ToolBandVisibility::KillWindowTimer(UINT timer_id) {
  KillTimer(timer_id);
}

void ToolBandVisibility::OnTimer(UINT_PTR nIDEvent) {
  DCHECK(nIDEvent == kVisibilityTimerId);
  KillWindowTimer(nIDEvent);

  if (!IsToolBandVisible(web_browser_)) {
    UnhideToolBand();
  }
  ClearCachedVisibility(web_browser_);
  CloseNotificationWindow();
}

void ToolBandVisibility::UnhideToolBand() {
  // Ignore ShowDW calls that are triggered by our calls here to
  // ShowBrowserBar.
  ceee_module_util::SetIgnoreShowDWChanges(true);
  CComVariant toolband_class(CLSID_ToolBand);
  CComVariant show(false);
  CComVariant empty;
  show = true;
  web_browser_->ShowBrowserBar(&toolband_class, &show, &empty);

  // Force IE to ignore bad caching.  This is a problem generally before IE7,
  // and at least sometimes even in IE7 (bb1291042).
  show = false;
  web_browser_->ShowBrowserBar(&toolband_class, &show, &empty);
  show = true;
  web_browser_->ShowBrowserBar(&toolband_class, &show, &empty);

  ceee_module_util::SetIgnoreShowDWChanges(false);
}
