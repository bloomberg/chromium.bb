// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_extended_context_menu_observer.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/search/search.h"

InstantExtendedContextMenuObserver::InstantExtendedContextMenuObserver(
    content::WebContents* contents)
    : is_instant_overlay_(chrome::IsInstantOverlay(contents)) {
}

InstantExtendedContextMenuObserver::~InstantExtendedContextMenuObserver() {
}

bool InstantExtendedContextMenuObserver::IsCommandIdSupported(int command_id) {
  switch (command_id) {
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_PRINT:
    case IDC_RELOAD:
      return is_instant_overlay_;
    default:
      return false;
  }
}

bool InstantExtendedContextMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  return false;
}
