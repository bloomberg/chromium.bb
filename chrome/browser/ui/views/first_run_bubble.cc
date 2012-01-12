// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/window.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {
const int kAnchorVerticalInset = 5;
const int kTopInset = 1;
const int kLeftInset = 2;
const int kBottomInset = 7;
const int kRightInset = 2;
}

// static
FirstRunBubble* FirstRunBubble::ShowBubble(Profile* profile,
                                           views::View* anchor_view) {
  FirstRunBubble* delegate = new FirstRunBubble(profile, anchor_view);
  browser::CreateViewsBubble(delegate);
  delegate->StartFade(true);
  return delegate;
}

void FirstRunBubble::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& original_font = rb.GetFont(ResourceBundle::MediumFont);

  views::Label* title = new views::Label(l10n_util::GetStringFUTF16(
      IDS_FR_BUBBLE_TITLE, GetDefaultSearchEngineName(profile_)));
  title->SetFont(original_font.DeriveFont(2, gfx::Font::BOLD));

  views::Link* change =
      new views::Link(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_CHANGE));
  change->SetFont(original_font);
  change->set_listener(this);

  views::Label* subtext =
      new views::Label(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_SUBTEXT));
  subtext->SetFont(original_font);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  layout->SetInsets(kTopInset, kLeftInset, kBottomInset, kRightInset);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);

  layout->StartRow(0, 0);
  layout->AddView(title);
  layout->AddView(change);
  layout->StartRowWithPadding(0, 0, 0,
      views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(subtext, columns->num_columns(), 1);
}

gfx::Rect FirstRunBubble::GetAnchorRect() {
  // Compensate for padding in anchor.
  gfx::Rect rect(BubbleDelegateView::GetAnchorRect());
  rect.Inset(0, anchor_view() ? kAnchorVerticalInset : 0);
  return rect;
}

FirstRunBubble::FirstRunBubble(Profile* profile, views::View* anchor_view)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      profile_(profile) {
}

FirstRunBubble::~FirstRunBubble() {
}

void FirstRunBubble::LinkClicked(views::Link* source, int event_flags) {
  // Get |profile_|'s browser before closing the bubble, which deletes |this|.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  GetWidget()->Close();
  if (browser)
    browser->OpenSearchEngineOptionsDialog();
}
