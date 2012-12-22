// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window_tab_helper.h"

#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ConstrainedWindowTabHelper)

ConstrainedWindowTabHelper::ConstrainedWindowTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(NULL) {
}

ConstrainedWindowTabHelper::~ConstrainedWindowTabHelper() {
  DCHECK(child_dialogs_.empty());
}

void ConstrainedWindowTabHelper::AddDialog(
    ConstrainedWindow* window) {
  child_dialogs_.push_back(window);

  if (child_dialogs_.size() == 1 && window->CanShowWebContentsModalDialog()) {
    window->ShowWebContentsModalDialog();
    BlockWebContentsInteraction(true);
  }
}

void ConstrainedWindowTabHelper::CloseAllDialogs() {
  // Clear out any web contents modal dialogs since we are leaving this page
  // entirely.  To ensure that we iterate over every element in child_dialogs_
  // we need to use a copy of child_dialogs_. Otherwise if
  // window->CloseWebContentsModalDialog() modifies child_dialogs_ we could end
  // up skipping some elements.
  WebContentsModalDialogList child_dialogs_copy(child_dialogs_);
  for (WebContentsModalDialogList::iterator it = child_dialogs_copy.begin();
       it != child_dialogs_copy.end(); ++it) {
    ConstrainedWindow* window = *it;
    if (window) {
      window->CloseWebContentsModalDialog();
      BlockWebContentsInteraction(false);
    }
  }
}

void ConstrainedWindowTabHelper::WillClose(ConstrainedWindow* window) {
  WebContentsModalDialogList::iterator i(
      std::find(child_dialogs_.begin(), child_dialogs_.end(), window));
  bool removed_topmost_window = i == child_dialogs_.begin();
  if (i != child_dialogs_.end())
    child_dialogs_.erase(i);
  if (child_dialogs_.empty()) {
    BlockWebContentsInteraction(false);
  } else {
    if (removed_topmost_window)
      child_dialogs_[0]->ShowWebContentsModalDialog();
    BlockWebContentsInteraction(true);
  }
}

void ConstrainedWindowTabHelper::BlockWebContentsInteraction(bool blocked) {
  WebContents* contents = web_contents();
  if (!contents) {
    // The WebContents has already disconnected.
    return;
  }

  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = contents->GetRenderViewHost();
  if (host) {
    host->SetIgnoreInputEvents(blocked);
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(
        host->GetRoutingID(), blocked));
  }
  if (delegate_)
    delegate_->SetWebContentsBlocked(contents, blocked);
}

void ConstrainedWindowTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Close constrained windows if necessary.
  if (!net::RegistryControlledDomainService::SameDomainOrHost(
          details.previous_url, details.entry->GetURL()))
    CloseAllDialogs();
}

void ConstrainedWindowTabHelper::DidGetIgnoredUIEvent() {
  if (dialog_count()) {
    ConstrainedWindow* window = *dialog_begin();
    window->FocusWebContentsModalDialog();
  }
}

void ConstrainedWindowTabHelper::WebContentsDestroyed(WebContents* tab) {
  // First cleanly close all child windows.
  // TODO(mpcomplete): handle case if MaybeCloseChildWindows() already asked
  // some of these to close.  CloseWindows is async, so it might get called
  // twice before it runs.
  CloseAllDialogs();
}
