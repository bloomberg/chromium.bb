// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/blocked_popup_container.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "gfx/rect.h"

// static
const size_t BlockedPopupContainer::kImpossibleNumberOfPopups = 30;

struct BlockedPopupContainer::BlockedPopup {
  BlockedPopup(TabContents* tab_contents,
               const gfx::Rect& bounds)
      : tab_contents(tab_contents), bounds(bounds) {
  }

  TabContents* tab_contents;
  gfx::Rect bounds;
};

BlockedPopupContainer::BlockedPopupContainer(TabContents* owner)
    : owner_(owner) {
}

void BlockedPopupContainer::AddTabContents(TabContents* tab_contents,
                                           const gfx::Rect& bounds) {
  if (blocked_popups_.size() == (kImpossibleNumberOfPopups - 1)) {
    delete tab_contents;
    LOG(INFO) << "Warning: Renderer is sending more popups to us than should "
        "be possible. Renderer compromised?";
    return;
  }

  blocked_popups_.push_back(BlockedPopup(tab_contents, bounds));
  tab_contents->set_delegate(this);
  if (blocked_popups_.size() == 1)
    owner_->PopupNotificationVisibilityChanged(true);
}

void BlockedPopupContainer::LaunchPopupForContents(TabContents* tab_contents) {
  // Open the popup.
  for (BlockedPopups::iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i) {
    if (i->tab_contents == tab_contents) {
      tab_contents->set_delegate(NULL);
      owner_->AddNewContents(tab_contents, NEW_POPUP, i->bounds, true);
      blocked_popups_.erase(i);
      break;
    }
  }

  if (blocked_popups_.empty())
    Destroy();
}

size_t BlockedPopupContainer::GetBlockedPopupCount() const {
  return blocked_popups_.size();
}

void BlockedPopupContainer::GetBlockedContents(
    BlockedContents* blocked_contents) const {
  DCHECK(blocked_contents);
  for (BlockedPopups::const_iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i)
    blocked_contents->push_back(i->tab_contents);
}

void BlockedPopupContainer::Destroy() {
  for (BlockedPopups::iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    tab_contents->set_delegate(NULL);
    delete tab_contents;
  }
  blocked_popups_.clear();
  owner_->WillCloseBlockedPopupContainer(this);
  delete this;
}

// Overridden from TabContentsDelegate:
void BlockedPopupContainer::OpenURLFromTab(TabContents* source,
                                           const GURL& url,
                                           const GURL& referrer,
                                           WindowOpenDisposition disposition,
                                           PageTransition::Type transition) {
  owner_->OpenURL(url, referrer, disposition, transition);
}

void BlockedPopupContainer::AddNewContents(TabContents* source,
                                           TabContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_position,
                                           bool user_gesture) {
  owner_->AddNewContents(new_contents, disposition, initial_position,
                         user_gesture);
}

void BlockedPopupContainer::CloseContents(TabContents* source) {
  for (BlockedPopups::iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    if (tab_contents == source) {
      tab_contents->set_delegate(NULL);
      blocked_popups_.erase(i);
      delete tab_contents;
      break;
    }
  }
}

void BlockedPopupContainer::MoveContents(TabContents* source,
                                         const gfx::Rect& new_bounds) {
  for (BlockedPopups::iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i) {
    if (i->tab_contents == source) {
      i->bounds = new_bounds;
      break;
    }
  }
}

bool BlockedPopupContainer::IsPopup(const TabContents* source) const {
  return true;
}

TabContents* BlockedPopupContainer::GetConstrainingContents(
    TabContents* source) {
  return owner_;
}
