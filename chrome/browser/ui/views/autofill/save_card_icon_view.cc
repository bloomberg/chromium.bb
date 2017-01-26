// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

namespace autofill {

SaveCardIconView::SaveCardIconView(CommandUpdater* command_updater,
                                   Browser* browser)
    : BubbleIconView(command_updater, IDC_SAVE_CREDIT_CARD_FOR_PAGE),
      browser_(browser) {
  set_id(VIEW_ID_SAVE_CREDIT_CARD_BUTTON);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD));
  if (browser)
    browser->tab_strip_model()->AddObserver(this);
}

SaveCardIconView::~SaveCardIconView() {}

void SaveCardIconView::OnExecuting(
    BubbleIconView::ExecuteSource execute_source) {}

views::BubbleDialogDelegateView* SaveCardIconView::GetBubble() const {
  SaveCardBubbleControllerImpl* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::SaveCardBubbleViews*>(
      controller->save_card_bubble_view());
}

const gfx::VectorIcon& SaveCardIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

void SaveCardIconView::TabDeactivated(content::WebContents* contents) {
  SaveCardBubbleControllerImpl* controller = GetController();
  if (controller)
    controller->HideBubble();
}

SaveCardBubbleControllerImpl* SaveCardIconView::GetController() const {
  if (!browser_)
    return nullptr;
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return nullptr;
  return autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

}  // namespace autofill
