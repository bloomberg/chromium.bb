// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/blocked_content_container.h"

#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/rect.h"

// static
const size_t BlockedContentContainer::kImpossibleNumberOfPopups = 30;

struct BlockedContentContainer::BlockedContent {
  BlockedContent(TabContents* tab_contents,
                 WindowOpenDisposition disposition,
                 const gfx::Rect& bounds,
                 bool user_gesture)
      : tab_contents(tab_contents),
        disposition(disposition),
        bounds(bounds),
        user_gesture(user_gesture) {
  }

  TabContents* tab_contents;
  WindowOpenDisposition disposition;
  gfx::Rect bounds;
  bool user_gesture;
};

BlockedContentContainer::BlockedContentContainer(TabContents* owner)
    : owner_(owner) {
}

BlockedContentContainer::~BlockedContentContainer() {}

void BlockedContentContainer::AddTabContents(TabContents* tab_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& bounds,
                                             bool user_gesture) {
  if (blocked_contents_.size() == (kImpossibleNumberOfPopups - 1)) {
    delete tab_contents;
    VLOG(1) << "Warning: Renderer is sending more popups to us than should be "
               "possible. Renderer compromised?";
    return;
  }

  blocked_contents_.push_back(
      BlockedContent(tab_contents, disposition, bounds, user_gesture));
  tab_contents->set_delegate(this);
  // Since the new tab_contents will not be showed, call WasHidden to change
  // its status on both RenderViewHost and RenderView.
  tab_contents->WasHidden();
  if (blocked_contents_.size() == 1)
    owner_->PopupNotificationVisibilityChanged(true);
}

void BlockedContentContainer::LaunchForContents(TabContents* tab_contents) {
  // Open the popup.
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    if (i->tab_contents == tab_contents) {
      // To support the owner blocking the content again we copy and erase
      // before attempting to add.
      BlockedContent content(*i);
      blocked_contents_.erase(i);
      i = blocked_contents_.end();
      tab_contents->set_delegate(NULL);
      // We needn't call WasRestored to change its status because the
      // TabContents::AddNewContents will do it.
      owner_->AddNewContents(tab_contents, content.disposition, content.bounds,
                             content.user_gesture);
      break;
    }
  }

  if (blocked_contents_.empty())
    Destroy();
}

size_t BlockedContentContainer::GetBlockedContentsCount() const {
  return blocked_contents_.size();
}

void BlockedContentContainer::GetBlockedContents(
    std::vector<TabContents*>* blocked_contents) const {
  DCHECK(blocked_contents);
  for (BlockedContents::const_iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i)
    blocked_contents->push_back(i->tab_contents);
}

void BlockedContentContainer::Destroy() {
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    tab_contents->set_delegate(NULL);
    delete tab_contents;
  }
  blocked_contents_.clear();
  owner_->WillCloseBlockedContentContainer(this);
  delete this;
}

// Overridden from TabContentsDelegate:
void BlockedContentContainer::OpenURLFromTab(TabContents* source,
                                             const GURL& url,
                                             const GURL& referrer,
                                             WindowOpenDisposition disposition,
                                             PageTransition::Type transition) {
  owner_->OpenURL(url, referrer, disposition, transition);
}

void BlockedContentContainer::AddNewContents(TabContents* source,
                                             TabContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_position,
                                             bool user_gesture) {
  owner_->AddNewContents(new_contents, disposition, initial_position,
                         user_gesture);
}

void BlockedContentContainer::CloseContents(TabContents* source) {
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    if (tab_contents == source) {
      tab_contents->set_delegate(NULL);
      blocked_contents_.erase(i);
      delete tab_contents;
      break;
    }
  }
}

void BlockedContentContainer::MoveContents(TabContents* source,
                                           const gfx::Rect& new_bounds) {
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    if (i->tab_contents == source) {
      i->bounds = new_bounds;
      break;
    }
  }
}

bool BlockedContentContainer::IsPopup(const TabContents* source) const {
  // Assume everything added is a popup. This may turn out to be wrong, but
  // callers don't cache this information so it should be fine if the value ends
  // up changing.
  return true;
}

TabContents* BlockedContentContainer::GetConstrainingContents(
    TabContents* source) {
  return owner_;
}
