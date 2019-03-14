// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ui_util.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

void UpdateLocalCardMigrationIcon(content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  // If feature is enabled, icon will be in the
  // ToolbarPageActionIconContainerView.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableToolbarStatusChip)) {
    PageActionIconContainer* toolbar_page_action_container =
        browser->window()->GetToolbarPageActionIconContainer();
    if (!toolbar_page_action_container)
      return;

    toolbar_page_action_container->UpdatePageActionIcon(
        PageActionIconType::kLocalCardMigration);
  } else {
    // Otherwise the icon will be in the LocationBar.
    LocationBar* location_bar = browser->window()->GetLocationBar();
    if (!location_bar)
      return;

    location_bar->UpdateLocalCardMigrationIcon();
  }
#endif
}

}  // namespace autofill
