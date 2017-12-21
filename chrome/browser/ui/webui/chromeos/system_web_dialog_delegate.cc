// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

SystemWebDialogDelegate::SystemWebDialogDelegate(const GURL& gurl,
                                                 const base::string16& title)
    : gurl_(gurl),
      title_(title),
      modal_type_(session_manager::SessionManager::Get()->session_state() ==
                          session_manager::SessionState::ACTIVE
                      ? ui::MODAL_TYPE_NONE
                      : ui::MODAL_TYPE_SYSTEM) {}

SystemWebDialogDelegate::~SystemWebDialogDelegate() {}

ui::ModalType SystemWebDialogDelegate::GetDialogModalType() const {
  return modal_type_;
}

base::string16 SystemWebDialogDelegate::GetDialogTitle() const {
  return title_;
}

GURL SystemWebDialogDelegate::GetDialogContentURL() const {
  return gurl_;
}

void SystemWebDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void SystemWebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidth, kDialogHeight);
}

void SystemWebDialogDelegate::OnDialogShown(
    content::WebUI* webui,
    content::RenderViewHost* render_view_host) {
  webui_ = webui;
}

void SystemWebDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SystemWebDialogDelegate::OnCloseContents(content::WebContents* source,
                                              bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool SystemWebDialogDelegate::ShouldShowDialogTitle() const {
  return !title_.empty();
}

void SystemWebDialogDelegate::ShowSystemDialog() {
  // NetworkConfigView does not interact well with web dialogs. For now, do
  // not show the dialog while NetworkConfigView is shown: crbug.com/791955.
  // TODO(stevenjb): Remove this when NetworkConfigView is deprecated.
  if (NetworkConfigView::HasInstance()) {
    delete this;
    return;
  }
  content::BrowserContext* browser_context =
      ProfileManager::GetActiveUserProfile();
  int container_id = GetDialogModalType() == ui::MODAL_TYPE_NONE
                         ? ash::kShellWindowId_AlwaysOnTopContainer
                         : ash::kShellWindowId_LockSystemModalContainer;
  chrome::ShowWebDialogInContainer(container_id, browser_context, this);
}

}  // namespace chromeos
