// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_OBSERVER_H_

#include "chrome/browser/tab_contents/navigation_controller.h"
#include "ipc/ipc_channel.h"

struct ViewHostMsg_FrameNavigate_Params;

// An observer API implemented by classes which are interested in various page
// load events from TabContents.  They also get a chance to filter IPC messages.
class TabContentsObserver : public IPC::Channel::Listener {
 public:
  virtual void NavigateToPendingEntry() { }

  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) { }
  virtual void DidNavigateAnyFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) { }

  virtual void DidStartLoading() { }
  virtual void DidStopLoading() { }

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

 protected:
  TabContentsObserver(TabContents* tab_contents);
  virtual ~TabContentsObserver();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message);

  TabContents* tab_contents() { return tab_contents_; }
  int routing_id() { return routing_id_; }

 private:
  friend class TabContents;

  void set_tab_contents(TabContents* tc) { tab_contents_ = tc; }

  TabContents* tab_contents_;
  // The routing ID of the associated TabContents.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsObserver);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_OBSERVER_H_
