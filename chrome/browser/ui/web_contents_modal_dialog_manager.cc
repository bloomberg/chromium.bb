// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"

#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(WebContentsModalDialogManager);

WebContentsModalDialogManager::~WebContentsModalDialogManager() {
  DCHECK(child_dialogs_.empty());
}

void WebContentsModalDialogManager::AddDialog(
    NativeWebContentsModalDialog dialog) {
  child_dialogs_.push_back(dialog);

  native_manager_->ManageDialog(dialog);

  if (child_dialogs_.size() == 1) {
    native_manager_->ShowDialog(dialog);
    BlockWebContentsInteraction(true);
  }
}

void WebContentsModalDialogManager::BlockWebContentsInteraction(bool blocked) {
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

bool WebContentsModalDialogManager::IsShowingDialog() const {
  return dialog_count() > 0;
}

void WebContentsModalDialogManager::FocusTopmostDialog() {
  DCHECK(dialog_count());
  native_manager_->FocusDialog(*dialog_begin());
}

void WebContentsModalDialogManager::WillClose(
    NativeWebContentsModalDialog dialog) {
  WebContentsModalDialogList::iterator i(
      std::find(child_dialogs_.begin(),
                child_dialogs_.end(),
                dialog));
  bool removed_topmost_dialog = i == child_dialogs_.begin();
  if (i != child_dialogs_.end())
    child_dialogs_.erase(i);
  if (child_dialogs_.empty()) {
    BlockWebContentsInteraction(false);
  } else {
    if (removed_topmost_dialog)
      native_manager_->ShowDialog(child_dialogs_[0]);
    BlockWebContentsInteraction(true);
  }
}

WebContentsModalDialogManager::WebContentsModalDialogManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(NULL),
      native_manager_(
          ALLOW_THIS_IN_INITIALIZER_LIST(CreateNativeManager(this))) {
  DCHECK(native_manager_);
}

void WebContentsModalDialogManager::CloseAllDialogs() {
  // Clear out any dialogs since we are leaving this page entirely.  To ensure
  // that we iterate over every element in child_dialogs_ we need to use a copy
  // of child_dialogs_. Otherwise if closing a dialog causes child_dialogs_ to
  // be modified, we could end up skipping some elements.
  WebContentsModalDialogList child_dialogs_copy(child_dialogs_);
  for (WebContentsModalDialogList::iterator it = child_dialogs_copy.begin();
       it != child_dialogs_copy.end(); ++it) {
    native_manager_->CloseDialog(*it);
    BlockWebContentsInteraction(false);
  }
}

void WebContentsModalDialogManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Close constrained windows if necessary.
  if (!net::RegistryControlledDomainService::SameDomainOrHost(
          details.previous_url, details.entry->GetURL()))
    CloseAllDialogs();
}

void WebContentsModalDialogManager::DidGetIgnoredUIEvent() {
  if (dialog_count())
    native_manager_->FocusDialog(*dialog_begin());
}

void WebContentsModalDialogManager::WebContentsDestroyed(WebContents* tab) {
  // First cleanly close all child dialogs.
  // TODO(mpcomplete): handle case if MaybeCloseChildWindows() already asked
  // some of these to close.  CloseAllDialogs is async, so it might get called
  // twice before it runs.
  CloseAllDialogs();
}
