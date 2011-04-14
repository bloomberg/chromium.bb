// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/choose_mobile_network_dialog.h"

#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/common/url_constants.h"

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 350;
const int kDefaultHeight = 225;

// Custom HtmlDialogView with disabled context menu.
class HtmlDialogWithoutContextMenuView : public HtmlDialogView {
 public:
  HtmlDialogWithoutContextMenuView(Profile* profile,
                                   HtmlDialogUIDelegate* delegate)
      : HtmlDialogView(profile, delegate) {}

  // TabContentsDelegate implementation.
  bool HandleContextMenu(const ContextMenuParams& params) {
    // Disable context menu.
    return true;
  }
};

}  // namespace

namespace chromeos {

// static
void ChooseMobileNetworkDialog::ShowDialog(gfx::NativeWindow owning_window) {
  Profile* profile;
  if (UserManager::Get()->user_is_logged_in()) {
    Browser* browser = BrowserList::GetLastActive();
    DCHECK(browser);
    profile = browser->profile();
  } else {
    profile = ProfileManager::GetDefaultProfile();
  }
  HtmlDialogView* html_view = new HtmlDialogWithoutContextMenuView(
      profile, new ChooseMobileNetworkDialog);
  html_view->InitDialog();
  chromeos::BubbleWindow::Create(owning_window,
                                 gfx::Rect(),
                                 chromeos::BubbleWindow::STYLE_GENERIC,
                                 html_view);
  html_view->window()->Show();
}

ChooseMobileNetworkDialog::ChooseMobileNetworkDialog() {
}

bool ChooseMobileNetworkDialog::IsDialogModal() const {
  return true;
}

std::wstring ChooseMobileNetworkDialog::GetDialogTitle() const {
  return std::wstring();
}

GURL ChooseMobileNetworkDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIChooseMobileNetworkURL);
}

void ChooseMobileNetworkDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void ChooseMobileNetworkDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string ChooseMobileNetworkDialog::GetDialogArgs() const {
  return "[]";
}

void ChooseMobileNetworkDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void ChooseMobileNetworkDialog::OnCloseContents(TabContents* source,
                                                bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool ChooseMobileNetworkDialog::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace chromeos
