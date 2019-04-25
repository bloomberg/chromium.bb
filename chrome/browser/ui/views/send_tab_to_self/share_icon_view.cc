// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/share_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

ShareIconView::ShareIconView(CommandUpdater* command_updater,
                             PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_SEND_TAB_TO_SELF, delegate) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_ICON_SEND_TAB_TO_SELF));
  SetUpForInOutAnimation();
}

ShareIconView::~ShareIconView() {}

views::BubbleDialogDelegateView* ShareIconView::GetBubble() const {
  // TODO(crbug/950388): add implementation.
  NOTIMPLEMENTED();
  return nullptr;
}

bool ShareIconView::Update() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }

  const bool was_visible = visible();
  const OmniboxView* omnibox_view = delegate()->GetOmniboxView();

  if (!send_tab_to_self::ShouldOfferFeature(web_contents) && was_visible) {
    // Hide the icon.
    ResetSlideAnimation(false);
    SetVisible(false);
  } else if (!was_visible && omnibox_view && omnibox_view->IsSelectAll()) {
    // Show the animation the first time the valid url is highlighted.
    AnimateIn(IDS_OMNIBOX_ICON_SEND_TAB_TO_SELF);
    SetVisible(true);
  }

  return was_visible != visible();
}

void ShareIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& ShareIconView::GetVectorIcon() const {
  return kSendTabToSelfIcon;
}

base::string16 ShareIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_TOOLTIP_SEND_TAB_TO_SELF);
}

void ShareIconView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);
}

}  // namespace send_tab_to_self
