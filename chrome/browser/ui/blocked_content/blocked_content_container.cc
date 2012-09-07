// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_content_container.h"

#include "base/logging.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/rect.h"

using content::OpenURLParams;
using content::WebContents;

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

BlockedContentContainer::~BlockedContentContainer() {
  Clear();
}

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
  tab_contents->web_contents()->SetDelegate(this);
  tab_contents->blocked_content_tab_helper()->set_delegate(this);
  // Since the new tab_contents will not be shown, call WasHidden to change
  // its status on both RenderViewHost and RenderView.
  tab_contents->web_contents()->WasHidden();
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
      tab_contents->web_contents()->SetDelegate(NULL);
      tab_contents->blocked_content_tab_helper()->set_delegate(NULL);
      // We needn't call WasShown to change its status because the
      // WebContents::AddNewContents will do it.
      WebContentsDelegate* delegate = owner_->web_contents()->GetDelegate();
      if (delegate) {
        delegate->AddNewContents(owner_->web_contents(),
                                 tab_contents->web_contents(),
                                 content.disposition,
                                 content.bounds,
                                 content.user_gesture,
                                 NULL);
      }
      break;
    }
  }
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

void BlockedContentContainer::Clear() {
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    tab_contents->web_contents()->SetDelegate(NULL);
    tab_contents->blocked_content_tab_helper()->set_delegate(NULL);
    delete tab_contents;
  }
  blocked_contents_.clear();
}

// Overridden from content::WebContentsDelegate:

WebContents* BlockedContentContainer::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  return owner_->web_contents()->OpenURL(params);
}

void BlockedContentContainer::AddNewContents(WebContents* source,
                                             WebContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_position,
                                             bool user_gesture,
                                             bool* was_blocked) {
  WebContentsDelegate* delegate = owner_->web_contents()->GetDelegate();
  if (delegate) {
    delegate->AddNewContents(owner_->web_contents(), new_contents, disposition,
                             initial_position, user_gesture, was_blocked);
  }
}

void BlockedContentContainer::CloseContents(WebContents* source) {
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    if (tab_contents->web_contents() == source) {
      tab_contents->web_contents()->SetDelegate(NULL);
      tab_contents->blocked_content_tab_helper()->set_delegate(NULL);
      blocked_contents_.erase(i);
      delete tab_contents;
      break;
    }
  }
}

void BlockedContentContainer::MoveContents(WebContents* source,
                                           const gfx::Rect& new_bounds) {
  for (BlockedContents::iterator i(blocked_contents_.begin());
       i != blocked_contents_.end(); ++i) {
    if (i->tab_contents->web_contents() == source) {
      i->bounds = new_bounds;
      break;
    }
  }
}

bool BlockedContentContainer::IsPopupOrPanel(const WebContents* source) const {
  // Assume everything added is a popup. This may turn out to be wrong, but
  // callers don't cache this information so it should be fine if the value ends
  // up changing.
  return true;
}

bool BlockedContentContainer::ShouldSuppressDialogs() {
  // Suppress JavaScript dialogs when inside a constrained popup window (because
  // that activates them and breaks them out of the constrained window jail).
  return true;
}

TabContents* BlockedContentContainer::GetConstrainingTabContents(
    TabContents* source) {
  return owner_;
}
