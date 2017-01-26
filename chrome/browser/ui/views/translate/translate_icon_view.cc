// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_icon_view.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

TranslateIconView::TranslateIconView(CommandUpdater* command_updater)
    : BubbleIconView(command_updater, IDC_TRANSLATE_PAGE) {
  set_id(VIEW_ID_TRANSLATE_BUTTON);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
}

TranslateIconView::~TranslateIconView() {}

void TranslateIconView::OnExecuting(
    BubbleIconView::ExecuteSource execute_source) {}

void TranslateIconView::OnPressed(bool activated) {
  translate::ReportUiAction((activated
                                 ? translate::PAGE_ACTION_ICON_ACTIVATED
                                 : translate::PAGE_ACTION_ICON_DEACTIVATED));
}

views::BubbleDialogDelegateView* TranslateIconView::GetBubble() const {
  return TranslateBubbleView::GetCurrentBubble();
}

const gfx::VectorIcon& TranslateIconView::GetVectorIcon() const {
  return kTranslateIcon;
}
