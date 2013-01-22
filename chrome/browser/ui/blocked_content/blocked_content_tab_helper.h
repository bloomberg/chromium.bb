// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_H_

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

class BlockedContentContainer;
class BlockedContentTabHelperDelegate;

// Per-tab class to manage blocked popups.
class BlockedContentTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BlockedContentTabHelper> {
 public:
  virtual ~BlockedContentTabHelper();

  BlockedContentTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(BlockedContentTabHelperDelegate* d) { delegate_ = d; }

  // Sets whether all WebContents added by way of |AddNewContents| should be
  // blocked. Transitioning from all blocked to not all blocked results in
  // reevaluating any blocked WebContentses, which may result in unblocking some
  // of the blocked WebContentses.
  void SetAllContentsBlocked(bool value);

  bool all_contents_blocked() const { return all_contents_blocked_; }

  // Adds the incoming |new_contents| to the |blocked_contents_| container.
  void AddWebContents(content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);

  // Adds the incoming |new_contents| to the |blocked_contents_| container.
  void AddPopup(content::WebContents* new_contents,
                WindowOpenDisposition disposition,
                const gfx::Rect& initial_pos,
                bool user_gesture);

  // Shows the blocked WebContents |web_contents|.
  void LaunchForContents(content::WebContents* web_contents);

  // Returns the number of blocked contents.
  size_t GetBlockedContentsCount() const;

  // Returns the blocked WebContentses.  |blocked_contents| must be non-NULL.
  void GetBlockedContents(
      std::vector<content::WebContents*>* blocked_contents) const;

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  explicit BlockedContentTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<BlockedContentTabHelper>;

  // Called when the blocked popup notification is shown or hidden.
  void PopupNotificationVisibilityChanged(bool visible);

  // Called to notify any observers that |contents| is entering or leaving
  // the blocked state.
  void SendNotification(content::WebContents* contents, bool blocked_state);

  // Object that holds any blocked WebContentses spawned from this WebContents.
  scoped_ptr<BlockedContentContainer> blocked_contents_;

  // Should we block all child WebContentses this attempts to spawn.
  bool all_contents_blocked_;

  // Delegate for notifying our owner (usually Browser) about stuff. Not owned
  // by us.
  BlockedContentTabHelperDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BlockedContentTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_TAB_HELPER_H_
