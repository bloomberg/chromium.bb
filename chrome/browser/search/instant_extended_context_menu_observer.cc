// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_extended_context_menu_observer.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/common/url_constants.h"

InstantExtendedContextMenuObserver::InstantExtendedContextMenuObserver(
    content::WebContents* contents, GURL url) : contents_(contents), url_(url) {
}

InstantExtendedContextMenuObserver::~InstantExtendedContextMenuObserver() {
}

bool InstantExtendedContextMenuObserver::IsCommandIdSupported(int command_id) {
  switch (command_id) {
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_PRINT:
    case IDC_RELOAD:
      return IsInstantOverlay();
    case IDC_CONTENT_CONTEXT_TRANSLATE:
      return IsLocalPage();
    default:
      return false;
  }
}

bool InstantExtendedContextMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  return false;
}

bool InstantExtendedContextMenuObserver::IsInstantOverlay() {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->instant_controller()) {
      content::WebContents* overlay_contents =
          it->instant_controller()->instant()->GetOverlayContents();
      if (overlay_contents && overlay_contents == contents_)
        return true;
    }
  }
  return false;
}

bool InstantExtendedContextMenuObserver::IsLocalPage() {
  return url_.host() == chrome::kChromeSearchLocalNtpHost;
}
