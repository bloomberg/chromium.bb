// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "content/public/browser/browser_thread.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

using content::BrowserThread;

namespace chromeos {

namespace {

bool TouchSupportAvailable(const display::Display& display) {
  return display.touch_support() ==
         display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE;
}

}  // namespace

OobeDisplayChooser::OobeDisplayChooser() : weak_ptr_factory_(this) {}

OobeDisplayChooser::~OobeDisplayChooser() {}

void OobeDisplayChooser::TryToPlaceUiOnTouchDisplay() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't (potentially) queue a second task to run MoveToTouchDisplay if one
  // already is queued.
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  if (primary_display.is_valid() && !TouchSupportAvailable(primary_display)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&OobeDisplayChooser::MoveToTouchDisplay,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OobeDisplayChooser::MoveToTouchDisplay() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const display::Displays& displays =
      ash::Shell::Get()->display_manager()->active_only_display_list();

  if (displays.size() <= 1)
    return;

  for (const display::Display& display : displays) {
    if (TouchSupportAvailable(display)) {
      ash::Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
          display.id());
      break;
    }
  }
}

}  // namespace chromeos
