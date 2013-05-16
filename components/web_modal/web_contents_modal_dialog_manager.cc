// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(web_modal::WebContentsModalDialogManager);

namespace web_modal {

WebContentsModalDialogManager::~WebContentsModalDialogManager() {
  DCHECK(child_dialogs_.empty());
}

void WebContentsModalDialogManager::ShowDialog(
    NativeWebContentsModalDialog dialog) {
  child_dialogs_.push_back(dialog);

  native_manager_->ManageDialog(dialog);

  if (child_dialogs_.size() == 1) {
    if (delegate_ && delegate_->IsWebContentsVisible(web_contents()))
      native_manager_->ShowDialog(dialog);
    BlockWebContentsInteraction(true);
  }
}

bool WebContentsModalDialogManager::IsShowingDialog() const {
  return !child_dialogs_.empty();
}

void WebContentsModalDialogManager::FocusTopmostDialog() {
  DCHECK(!child_dialogs_.empty());
  native_manager_->FocusDialog(child_dialogs_.front());
}

content::WebContents* WebContentsModalDialogManager::GetWebContents() const {
  return web_contents();
}

void WebContentsModalDialogManager::WillClose(
    NativeWebContentsModalDialog dialog) {
  WebContentsModalDialogList::iterator i(
      std::find(child_dialogs_.begin(), child_dialogs_.end(), dialog));

  // The Views tab contents modal dialog calls WillClose twice.  Ignore the
  // second invocation.
  if (i == child_dialogs_.end())
    return;

  bool removed_topmost_dialog = i == child_dialogs_.begin();
  child_dialogs_.erase(i);
  if (!child_dialogs_.empty() && removed_topmost_dialog &&
      !closing_all_dialogs_)
    native_manager_->ShowDialog(child_dialogs_.front());

  BlockWebContentsInteraction(!child_dialogs_.empty());
}

void WebContentsModalDialogManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED);

  if (child_dialogs_.empty())
    return;

  bool visible = *content::Details<bool>(details).ptr();
  if (visible)
    native_manager_->ShowDialog(child_dialogs_.front());
  else
    native_manager_->HideDialog(child_dialogs_.front());
}

WebContentsModalDialogManager::WebContentsModalDialogManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(NULL),
      native_manager_(CreateNativeManager(this)),
      closing_all_dialogs_(false) {
  DCHECK(native_manager_);
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 content::Source<content::WebContents>(web_contents));
}

void WebContentsModalDialogManager::BlockWebContentsInteraction(bool blocked) {
  WebContents* contents = web_contents();
  if (!contents) {
    // The WebContents has already disconnected.
    return;
  }

  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = contents->GetRenderViewHost();
  if (host)
    host->SetIgnoreInputEvents(blocked);
  if (delegate_)
    delegate_->SetWebContentsBlocked(contents, blocked);
}

void WebContentsModalDialogManager::CloseAllDialogs() {
  closing_all_dialogs_ = true;

  // Clear out any dialogs since we are leaving this page entirely.
  while (!child_dialogs_.empty())
    native_manager_->CloseDialog(child_dialogs_.front());

  closing_all_dialogs_ = false;
}

void WebContentsModalDialogManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Close constrained windows if necessary.
  if (!net::registry_controlled_domains::SameDomainOrHost(
          details.previous_url, details.entry->GetURL(),
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES))
    CloseAllDialogs();
}

void WebContentsModalDialogManager::DidGetIgnoredUIEvent() {
  if (!child_dialogs_.empty())
    native_manager_->FocusDialog(child_dialogs_.front());
}

void WebContentsModalDialogManager::WebContentsDestroyed(WebContents* tab) {
  // First cleanly close all child dialogs.
  // TODO(mpcomplete): handle case if MaybeCloseChildWindows() already asked
  // some of these to close.  CloseAllDialogs is async, so it might get called
  // twice before it runs.
  CloseAllDialogs();
}

}  // namespace web_modal
