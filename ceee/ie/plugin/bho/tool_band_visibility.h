// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEEE_IE_PLUGIN_BHO_TOOL_BAND_VISIBILITY_H_
#define CEEE_IE_PLUGIN_BHO_TOOL_BAND_VISIBILITY_H_

#include <atlbase.h>
#include <atlcrack.h>
#include <atlwin.h>
#include <mshtml.h>  // Needed for exdisp.h
#include <exdisp.h>
#include <set>

#include "base/basictypes.h"

// The ToolBandVisibility class allows us to recover from a number of
// features of IE that can cause our toolband to become invisible
// unexpectedly.  See the .cc file for more details.
class ATL_NO_VTABLE ToolBandVisibility
    : public CWindowImpl<ToolBandVisibility> {
 public:
  // Inform ToolBandVisibility that the toolband has been created for this
  // browser instance.
  static void ReportToolBandVisible(IWebBrowser2* web_browser);

  BEGIN_MSG_MAP(ToolBandVisibility)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_TIMER(OnTimer)
  END_MSG_MAP()

 protected:
  // Returns true iff ReportToolBandVisible has been called for the given web
  // browser.
  static bool IsToolBandVisible(IWebBrowser2* web_browser);

  // Cleans up the visibility set entry for the given browser that was stored
  // when the browser called ReportToolBandVisible.  If the pointer passed in is
  // NULL, all items in the entire visibility set are deleted (this is useful
  // for testing).
  static void ClearCachedVisibility(IWebBrowser2* web_browser);

  ToolBandVisibility();
  virtual ~ToolBandVisibility();

  // Checks toolband visibility, and forces the toolband to be shown if it
  // isn't, and the user hasn't explicitly hidden the toolband.
  void CheckToolBandVisibility(IWebBrowser2* web_browser);

  // Cleans up toolband visibility data when the BHO is being torn down.
  void TearDown();

  // Set up the notification window used for processing ToolBandVisibilityWindow
  // messages.
  // Unfortunately, we need to create a notification window to handle a delayed
  // check for the toolband. Other methods (like creating a new thread and
  // sleeping) will not work.
  // Returns true on success.
  // Also serves as a unit testing seam.
  virtual bool CreateNotificationWindow();

  // Unit testing seam for destroying the window.
  virtual void CloseNotificationWindow();

  // Unit testing seam for setting the timer for the visibility window.
  virtual void SetWindowTimer(UINT timer_id, UINT delay);

  // Unit testing seam for killing the timer for the visibility window.
  virtual void KillWindowTimer(UINT timer_id);

  // @name Message handlers.
  // @{
  // The OnCreate handler is empty; subclasses can override it for more
  // functionality.
  virtual LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct) {
    return 0;
  }
  void OnTimer(UINT_PTR nIDEvent);
  // @}

  // Forces the toolband to be shown.
  void UnhideToolBand();

  // The web browser instance for which we are tracking toolband visibility.
  CComPtr<IWebBrowser2> web_browser_;

 private:
  // The set of browser windows that have visible toolbands.
  // The BHO for each browser window is responsible for cleaning up its
  // own entry in this set when it's torn down; see ClearToolBandVisibility.
  // This collection does not hold references to the objects it stores.
  static std::set<IUnknown*> visibility_set_;
  static CComAutoCriticalSection visibility_set_crit_;

  DISALLOW_COPY_AND_ASSIGN(ToolBandVisibility);
};

#endif  // CEEE_IE_PLUGIN_BHO_TOOL_BAND_VISIBILITY_H_
