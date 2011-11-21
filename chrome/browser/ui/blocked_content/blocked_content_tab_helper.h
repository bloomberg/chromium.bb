// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_H_
#pragma once

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/glue/window_open_disposition.h"

class BlockedContentContainer;
class BlockedContentTabHelperDelegate;
class TabContentsWrapper;

// Per-tab class to manage blocked popups.
class BlockedContentTabHelper : public TabContentsObserver {
 public:
  explicit BlockedContentTabHelper(TabContentsWrapper* tab_contents);
  virtual ~BlockedContentTabHelper();

  BlockedContentTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(BlockedContentTabHelperDelegate* d) { delegate_ = d; }

  // Sets whether all TabContents added by way of |AddNewContents| should be
  // blocked. Transitioning from all blocked to not all blocked results in
  // reevaluating any blocked TabContents, which may result in unblocking some
  // of the blocked TabContents.
  void SetAllContentsBlocked(bool value);

  bool all_contents_blocked() const { return all_contents_blocked_; }

  // Adds the incoming |new_contents| to the |blocked_contents_| container.
  void AddTabContents(TabContentsWrapper* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);

  // Adds the incoming |new_contents| to the |blocked_contents_| container.
  void AddPopup(TabContentsWrapper* new_contents,
                const gfx::Rect& initial_pos,
                bool user_gesture);

  // Shows the blocked TabContents |tab_contents|.
  void LaunchForContents(TabContentsWrapper* tab_contents);

  // Returns the number of blocked contents.
  size_t GetBlockedContentsCount() const;

  // Returns the blocked TabContentsWrappers.  |blocked_contents| must
  // be non-NULL.
  void GetBlockedContents(
      std::vector<TabContentsWrapper*>* blocked_contents) const;

  // TabContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Called when the blocked popup notification is shown or hidden.
  void PopupNotificationVisibilityChanged(bool visible);

  // Object that holds any blocked TabContents spawned from this TabContents.
  scoped_ptr<BlockedContentContainer> blocked_contents_;

  // Should we block all child TabContents this attempts to spawn.
  bool all_contents_blocked_;

  // Owning TabContentsWrapper.
  TabContentsWrapper* tab_contents_wrapper_;

  // Delegate for notifying our owner (usually Browser) about stuff. Not owned
  // by us.
  BlockedContentTabHelperDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BlockedContentTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_H_
