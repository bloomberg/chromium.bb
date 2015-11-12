// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icons_public.h"

TranslateIconView::TranslateIconView(CommandUpdater* command_updater)
    : BubbleIconView(command_updater, IDC_TRANSLATE_PAGE) {
  set_id(VIEW_ID_TRANSLATE_BUTTON);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
}

TranslateIconView::~TranslateIconView() {}

void TranslateIconView::OnExecuting(
    BubbleIconView::ExecuteSource execute_source) {}

views::BubbleDelegateView* TranslateIconView::GetBubble() const {
  return TranslateBubbleView::GetCurrentBubble();
}

gfx::VectorIconId TranslateIconView::GetVectorIcon() const {
  return gfx::VectorIconId::TRANSLATE;
}
