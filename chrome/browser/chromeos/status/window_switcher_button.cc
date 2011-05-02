// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/window_switcher_button.h"

#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"

namespace chromeos {

namespace {
int GetNormalBrowserCount() {
  int count = 0;
  BrowserList::const_iterator iter;
  for (iter = BrowserList::begin(); iter != BrowserList::end(); ++iter) {
    if ((*iter)->type() == Browser::TYPE_NORMAL)
      count++;
  }
  return count;
}
}

WindowSwitcherButton::WindowSwitcherButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WINDOW_SWITCHER));
  SetEnabled(true);

  UpdateStatus();
  BrowserList::AddObserver(this);
}

WindowSwitcherButton::~WindowSwitcherButton() {
  BrowserList::RemoveObserver(this);
}

void WindowSwitcherButton::UpdateStatus() {
  if (GetNormalBrowserCount() < 2) {
    // There are less than two browsers.  This means we can't switch
    // anywhere, so we disappear.
    SetTooltipText(L"");
    SetVisible(false);
    PreferredSizeChanged();
    return;
  }

  SetTooltipText(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_STATUSBAR_WINDOW_SWITCHER_TOOLTIP)));

  // There are at least two browsers, so we show ourselves.
  SetVisible(true);
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// WindowSwitcherButton, views::ViewMenuDelegate implementation:

void WindowSwitcherButton::RunMenu(views::View* source, const gfx::Point& pt) {
  // Don't do anything if there aren't at least two normal browsers to
  // switch between.
  if (GetNormalBrowserCount() < 2)
    return;

  // Send a message to the ChromeOS window manager to switch to the
  // next top level browser window.  Only the window manager knows
  // what order they are in, so we can't use Chrome's browser list
  // to decide.
  WmIpc::Message message(chromeos::WM_IPC_MESSAGE_WM_CYCLE_WINDOWS);
  message.set_param(0, true);
  WmIpc::instance()->SendMessage(message);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserList::Observer implementation:

// Called immediately after a browser is added to the list
void WindowSwitcherButton::OnBrowserAdded(const Browser* browser) {
  UpdateStatus();
}

// Called immediately after a browser is removed from the list
void WindowSwitcherButton::OnBrowserRemoved(const Browser* browser) {
  UpdateStatus();
}

}  // namespace chromeos
