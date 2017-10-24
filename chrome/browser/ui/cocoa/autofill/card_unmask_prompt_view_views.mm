// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/ui/cocoa/autofill/card_unmask_prompt_view_bridge.h"
#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#include "chrome/browser/ui/views/autofill/card_unmask_prompt_views.h"
#include "components/autofill/core/browser/autofill_experiments.h"

namespace autofill {

CardUnmaskPromptView* CreateCardUnmaskPromptView(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(kAutofillToolkitViewsCreditCardDialogsMac))
    return new CardUnmaskPromptViews(controller, web_contents);
  return new CardUnmaskPromptViewBridge(controller, web_contents);
}

}
