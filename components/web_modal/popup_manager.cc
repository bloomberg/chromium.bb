// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/popup_manager.h"

#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;

namespace web_modal {

namespace {

const char kPopupManagerUserDataKey[] = "PopupManager";

// This class provides a hook to get a PopupManager from a WebContents.
// The PopupManager is browser-scoped, but will use a FromWebContents API
// to attach to each WebContents in that browser.
class PopupManagerRelay : public content::WebContentsUserData<PopupManager> {
 public:
  explicit PopupManagerRelay(base::WeakPtr<PopupManager> manager)
      : manager_(manager) {}

  virtual ~PopupManagerRelay() {}

  base::WeakPtr<PopupManager> manager_;
};

}  // namespace

PopupManager::PopupManager(WebContentsModalDialogHost* host)
    : host_(host),
      weak_factory_(this) {}

PopupManager::~PopupManager() {
}

void PopupManager::ShowPopup(scoped_ptr<SinglePopupManager> manager) {
  content::WebContents* web_contents = manager->GetBoundWebContents();
  // TODO(gbillock): get rid of this when we handle bubbles
  DCHECK(web_contents);

  // TODO(gbillock): remove when we port the popup management logic to this
  // class.
  NativeWebContentsModalDialog dialog =
      static_cast<NativeWebContentsModalDialog>(manager->popup());

  WebContentsModalDialogManager* wm_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  DCHECK(wm_manager);
  wm_manager->ShowModalDialog(dialog);
}

void PopupManager::ShowModalDialog(NativePopup popup,
                                   content::WebContents* web_contents) {
  // TODO make a new native popup manager and call ShowPopup.
  // For now just lay off to WCMDM.
  WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  manager->ShowModalDialog(popup);
}

bool PopupManager::IsWebModalDialogActive(
    const content::WebContents* web_contents) const {
  if (web_contents == NULL)
    return false;

  const WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  return manager && manager->IsDialogActive();
}

void PopupManager::WasFocused(const content::WebContents* web_contents) {
  if (!IsWebModalDialogActive(web_contents))
    return;

  const WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  if (manager)
    manager->FocusTopmostDialog();
}

void PopupManager::WillClose(NativePopup popup) {
}

void PopupManager::RegisterWith(content::WebContents* web_contents) {
  web_contents->SetUserData(kPopupManagerUserDataKey,
                            new PopupManagerRelay(weak_factory_.GetWeakPtr()));
  // TODO(gbillock): Need to do something more extreme here to manage changing
  // popup managers with popups in-flight?
}

void PopupManager::UnregisterWith(content::WebContents* web_contents) {
  web_contents->RemoveUserData(kPopupManagerUserDataKey);
  // TODO(gbillock): Need to do something more extreme here to manage changing
  // popup managers with popups in-flight?
}

PopupManager* PopupManager::FromWebContents(
    content::WebContents* web_contents) {
  PopupManagerRelay* relay = static_cast<PopupManagerRelay*>(
      web_contents->GetUserData(kPopupManagerUserDataKey));
  if (!relay)
    return NULL;

  return relay->manager_.get();
}

gfx::NativeView PopupManager::GetHostView() const {
  // TODO(gbillock): replace this with a PopupManagerHost or something.
  DCHECK(host_);
  return host_->GetHostView();
}

void PopupManager::CloseAllDialogsForTesting(
    content::WebContents* web_contents) {
  // TODO: re-implement, probably in terms of something in the host_,
  // or of owned WCMDMs.
  WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  manager->CloseAllDialogs();
}

}  // namespace web_modal
