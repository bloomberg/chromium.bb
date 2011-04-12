// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sim_unlock_dialog_delegate.h"

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/common/url_constants.h"

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 350;
const int kDefaultHeight = 225;

// URL that includes new value for the RequirePin SIM preference.
// In general SIM unlock case sim-unlock URL is loaded w/o params.
const char kSimUnlockRequirePinPrefURL[] =
    "chrome://sim-unlock/?pin-req=%s";

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
void SimUnlockDialogDelegate::ShowDialog(gfx::NativeWindow owning_window) {
  SimUnlockDialogDelegate::ShowDialog(owning_window, NOT_CHANGED);
}

// static
void SimUnlockDialogDelegate::ShowDialog(gfx::NativeWindow owning_window,
                                         bool require_pin) {
  SimUnlockDialogDelegate::ShowDialog(
      owning_window, require_pin ? PIN_REQUIRED : PIN_NOT_REQUIRED);
}

// static
void SimUnlockDialogDelegate::ShowDialog(gfx::NativeWindow owning_window,
                                         SimRequirePin require_pin) {
  Browser* browser = BrowserList::GetLastActive();
  HtmlDialogView* html_view = new HtmlDialogWithoutContextMenuView(
      browser->profile(), new SimUnlockDialogDelegate(require_pin));
  html_view->InitDialog();
  chromeos::BubbleWindow::Create(owning_window,
                                 gfx::Rect(),
                                 chromeos::BubbleWindow::STYLE_GENERIC,
                                 html_view);
  html_view->window()->Show();
}

SimUnlockDialogDelegate::SimUnlockDialogDelegate()
    : require_pin_(NOT_CHANGED) {
}

SimUnlockDialogDelegate::SimUnlockDialogDelegate(SimRequirePin require_pin)
    : require_pin_(require_pin) {
}

SimUnlockDialogDelegate::~SimUnlockDialogDelegate() {
}

bool SimUnlockDialogDelegate::IsDialogModal() const {
  return true;
}

std::wstring SimUnlockDialogDelegate::GetDialogTitle() const {
  return std::wstring();
}

GURL SimUnlockDialogDelegate::GetDialogContentURL() const {
  if (require_pin_ == NOT_CHANGED) {
    std::string url_string(chrome::kChromeUISimUnlockURL);
    return GURL(url_string);
  } else {
    std::string value(require_pin_ == PIN_REQUIRED ? "true" : "false");
    return GURL(StringPrintf(kSimUnlockRequirePinPrefURL, value.c_str()));
  }
}

void SimUnlockDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void SimUnlockDialogDelegate::GetDialogSize(gfx::Size* size) const {
  // TODO(nkostylev): Set custom size based on locale settings.
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string SimUnlockDialogDelegate::GetDialogArgs() const {
  return "[]";
}

void SimUnlockDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SimUnlockDialogDelegate::OnCloseContents(TabContents* source,
                                              bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool SimUnlockDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace chromeos
