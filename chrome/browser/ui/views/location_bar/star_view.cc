// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icons_public.h"

StarView::StarView(CommandUpdater* command_updater, Browser* browser)
    : BubbleIconView(command_updater, IDC_BOOKMARK_PAGE), browser_(browser) {
  set_id(VIEW_ID_STAR_BUTTON);
  SetToggled(false);
}

StarView::~StarView() {}

void StarView::SetToggled(bool on) {
  BubbleIconView::SetActiveInternal(on);
  SetTooltipText(l10n_util::GetStringUTF16(
      on ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR));
}

void StarView::OnExecuting(
    BubbleIconView::ExecuteSource execute_source) {
  BookmarkEntryPoint entry_point = BOOKMARK_ENTRY_POINT_STAR_MOUSE;
  switch (execute_source) {
    case EXECUTE_SOURCE_MOUSE:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_MOUSE;
      break;
    case EXECUTE_SOURCE_KEYBOARD:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_KEY;
      break;
    case EXECUTE_SOURCE_GESTURE:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_GESTURE;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                            entry_point,
                            BOOKMARK_ENTRY_POINT_LIMIT);
}

void StarView::ExecuteCommand(ExecuteSource source) {
  if (browser_) {
    OnExecuting(source);
    chrome::BookmarkCurrentPageIgnoringExtensionOverrides(browser_);
  } else {
    BubbleIconView::ExecuteCommand(source);
  }
}

views::BubbleDelegateView* StarView::GetBubble() const {
  return BookmarkBubbleView::bookmark_bubble();
}

bool StarView::SetRasterIcon() {
  if (ui::MaterialDesignController::IsModeMaterial())
    return false;

  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      active() ? IDR_STAR_LIT : IDR_STAR));
  return true;
}

gfx::VectorIconId StarView::GetVectorIcon() const {
  return active() ? gfx::VectorIconId::LOCATION_BAR_STAR_ACTIVE
                  : gfx::VectorIconId::LOCATION_BAR_STAR;
}
