// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/window_switcher_button.h"

#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"

namespace chromeos {

WindowSwitcherButton::WindowSwitcherButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      host_(host) {
  UpdateStatus();
  BrowserList::AddObserver(this);
}

WindowSwitcherButton::~WindowSwitcherButton() {
  BrowserList::RemoveObserver(this);
}

void WindowSwitcherButton::UpdateStatus() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // If there's more than one window, we enable ourselves.
  if (BrowserList::size() > 1) {
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WINDOW_SWITCHER));
    SetEnabled(true);
  } else {
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WINDOW_SWITCHER_DISABLED));
    SetEnabled(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowSwitcherButton, views::ViewMenuDelegate implementation:

void WindowSwitcherButton::RunMenu(views::View* source, const gfx::Point& pt) {
  if (BrowserList::empty())
    return;
  Browser* last_active = BrowserList::GetLastActive();
  if (last_active) {
    BrowserList::const_iterator iter;
    for (iter = BrowserList::begin(); iter != BrowserList::end(); ++iter) {
      if (last_active == *iter) {
        ++iter;
        break;
      }
    }
    if (iter == BrowserList::end())
      iter = BrowserList::begin();
    (*iter)->window()->Activate();
  } else {
    (*BrowserList::begin())->window()->Activate();
  }
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
