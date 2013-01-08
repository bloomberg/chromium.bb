// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_content_container.h"

#include "base/logging.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/rect.h"

using content::OpenURLParams;
using content::WebContents;

// static
const size_t BlockedContentContainer::kImpossibleNumberOfPopups = 30;

struct BlockedContentContainer::BlockedContent {
  BlockedContent(WebContents* web_contents,
                 WindowOpenDisposition disposition,
                 const gfx::Rect& bounds,
                 bool user_gesture)
      : web_contents(web_contents),
        disposition(disposition),
        bounds(bounds),
        user_gesture(user_gesture) {
  }

  WebContents* web_contents;
  WindowOpenDisposition disposition;
  gfx::Rect bounds;
  bool user_gesture;
};

BlockedContentContainer::BlockedContentContainer(content::WebContents* owner)
    : owner_(owner) {
}

BlockedContentContainer::~BlockedContentContainer() {
  Clear();
}

void BlockedContentContainer::AddWebContents(content::WebContents* web_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& bounds,
                                             bool user_gesture) {
  if (blocked_contents_.size() == (kImpossibleNumberOfPopups - 1)) {
    delete web_contents;
    VLOG(1) << "Warning: Renderer is sending more popups to us than should be "
               "possible. Renderer compromised?";
    return;
  }

  blocked_contents_.push_back(
      BlockedContent(web_contents, disposition, bounds, user_gesture));
  web_contents->SetDelegate(this);

  BlockedContentTabHelper::CreateForWebContents(web_contents);
  // Various UI parts assume the ability to retrieve favicons from blocked
  // content.
  FaviconTabHelper::CreateForWebContents(web_contents);
  BlockedContentTabHelper::FromWebContents(web_contents)->set_delegate(this);

  // Since the new web_contents will not be shown, call WasHidden to change
  // its status on both RenderViewHost and RenderView.
  web_contents->WasHidden();
}

void BlockedContentContainer::LaunchForContents(
    content::WebContents* web_contents) {
  // Open the popup.
  for (BlockedContents::iterator i = blocked_contents_.begin();
       i != blocked_contents_.end(); ++i) {
    if (i->web_contents == web_contents) {
      // To support the owner blocking the content again we copy and erase
      // before attempting to add.
      BlockedContent content(*i);
      blocked_contents_.erase(i);
      i = blocked_contents_.end();
      web_contents->SetDelegate(NULL);
      BlockedContentTabHelper::FromWebContents(web_contents)->
          set_delegate(NULL);
      // We needn't call WasShown to change its status because the
      // WebContents::AddNewContents will do it.
      WebContentsDelegate* delegate = owner_->GetDelegate();
      if (delegate) {
        delegate->AddNewContents(owner_,
                                 web_contents,
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
    std::vector<WebContents*>* blocked_contents) const {
  DCHECK(blocked_contents);
  for (BlockedContents::const_iterator i = blocked_contents_.begin();
       i != blocked_contents_.end(); ++i)
    blocked_contents->push_back(i->web_contents);
}

void BlockedContentContainer::Clear() {
  for (BlockedContents::iterator i = blocked_contents_.begin();
       i != blocked_contents_.end(); ++i) {
    WebContents* web_contents = i->web_contents;
    web_contents->SetDelegate(NULL);
    BlockedContentTabHelper::FromWebContents(web_contents)->set_delegate(NULL);
    delete web_contents;
  }
  blocked_contents_.clear();
}

// Overridden from content::WebContentsDelegate:

WebContents* BlockedContentContainer::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  return owner_->OpenURL(params);
}

void BlockedContentContainer::AddNewContents(WebContents* source,
                                             WebContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_position,
                                             bool user_gesture,
                                             bool* was_blocked) {
  WebContentsDelegate* delegate = owner_->GetDelegate();
  if (delegate) {
    delegate->AddNewContents(owner_, new_contents, disposition,
                             initial_position, user_gesture, was_blocked);
  }
}

void BlockedContentContainer::CloseContents(WebContents* source) {
  for (BlockedContents::iterator i = blocked_contents_.begin();
       i != blocked_contents_.end(); ++i) {
    WebContents* web_contents = i->web_contents;
    if (web_contents == source) {
      web_contents->SetDelegate(NULL);
      BlockedContentTabHelper::FromWebContents(web_contents)->
          set_delegate(NULL);
      blocked_contents_.erase(i);
      delete web_contents;
      break;
    }
  }
}

void BlockedContentContainer::MoveContents(WebContents* source,
                                           const gfx::Rect& new_bounds) {
  for (BlockedContents::iterator i = blocked_contents_.begin();
       i != blocked_contents_.end(); ++i) {
    if (i->web_contents == source) {
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

content::WebContents* BlockedContentContainer::GetConstrainingWebContents(
    content::WebContents* source) {
  return owner_;
}
