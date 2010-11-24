// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_NAVIGATION_OBSERVER_H_

#include "chrome/browser/tab_contents/navigation_controller.h"

struct ViewHostMsg_FrameNavigate_Params;

// An observer API implemented by classes which are interested in various page
// load events from TabContents.

// TODO(pink): Is it worth having a bitfield where certain clients can only
// register for certain events? Is the extra function call worth the added pain
// on the caller to build the bitfield?

class WebNavigationObserver {
 public:
  // For removing PasswordManager deps...

  virtual void NavigateToPendingEntry() { }

  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) { }
  virtual void DidNavigateAnyFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) { }

  virtual void DidStartLoading() { }
  virtual void DidStopLoading() { }

  // TODO(beng): These should move from here once PasswordManager is able to
  //             establish its own private communication protocol to the
  //             renderer.
  virtual void PasswordFormsFound(
      const std::vector<webkit_glue::PasswordForm>& forms) { }
  virtual void PasswordFormsVisible(
      const std::vector<webkit_glue::PasswordForm>& visible_forms) { }

#if 0
  // For unifying with delegate...

  // Notifies the delegate that this contents is starting or is done loading
  // some resource. The delegate should use this notification to represent
  // loading feedback. See TabContents::is_loading()
  virtual void LoadingStateChanged(TabContents* contents) { }
  // Called to inform the delegate that the tab content's navigation state
  // changed. The |changed_flags| indicates the parts of the navigation state
  // that have been updated, and is any combination of the
  // |TabContents::InvalidateTypes| bits.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) { }
#endif
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_NAVIGATION_OBSERVER_H_
