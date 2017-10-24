// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/ui/cocoa/autofill/save_card_bubble_view_bridge.h"
#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/save_credit_card_decoration.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace autofill {

SaveCardBubbleView* CreateSaveCardBubbleView(
    content::WebContents* web_contents,
    autofill::SaveCardBubbleController* controller,
    BrowserWindowController* browser_window_controller,
    bool user_gesture) {
  if (base::FeatureList::IsEnabled(kAutofillToolkitViewsCreditCardDialogsMac)) {
    LocationBarViewMac* location_bar =
        [browser_window_controller locationBarBridge];
    gfx::Point anchor =
        gfx::ScreenPointFromNSPoint(ui::ConvertPointFromWindowToScreen(
            [browser_window_controller window],
            location_bar->GetSaveCreditCardBubblePoint()));
    autofill::SaveCardBubbleViews* bubble =
        new SaveCardBubbleViews(nullptr, anchor, web_contents, controller);

    // Usually the anchor view determines the arrow type, but there is none in
    // MacViews. So always use TOP_RIGHT, even in fullscreen.
    bubble->set_arrow(views::BubbleBorder::TOP_RIGHT);
    bubble->set_parent_window(web_contents->GetNativeView());
    views::BubbleDialogDelegateView::CreateBubble(bubble);
    bubble->Show(user_gesture ? autofill::SaveCardBubbleViews::USER_GESTURE
                              : autofill::SaveCardBubbleViews::AUTOMATIC);
    KeepBubbleAnchored(bubble, location_bar->save_credit_card_decoration());
    return bubble;
  }
  return new SaveCardBubbleViewBridge(controller, browser_window_controller);
}
}
