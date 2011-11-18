// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_bubble.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"

namespace {
const int kAnchorVerticalOffset = -4;
const int kLayoutTopInset = 1;
const int kLayoutLeftInset = 2;
const int kLayoutBottomInset = 7;
const int kLayoutRightInset = 2;
}

// static
FirstRunBubble* FirstRunBubble::ShowBubble(
    Profile* profile,
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    FirstRun::BubbleType bubble_type) {
  FirstRunBubble* delegate =
      new FirstRunBubble(profile,
                         anchor_view,
                         arrow_location,
                         bubble_type);
  views::BubbleDelegateView::CreateBubble(delegate);
  delegate->StartFade(true);
  return delegate;
}

void FirstRunBubble::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& original_font = rb.GetFont(ResourceBundle::MediumFont);
  const gfx::Font& derived_font = original_font.DeriveFont(2, gfx::Font::BOLD);

  views::Label* label1 = new views::Label(l10n_util::GetStringFUTF16(
      IDS_FR_SE_BUBBLE_TITLE,
      GetDefaultSearchEngineName(profile_)));
  label1->SetFont(derived_font);

  views::Label* label2 =
      new views::Label(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_SUBTEXT));
  label2->SetFont(original_font);

  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                         rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button->SetImage(views::CustomButton::BS_HOT,
                         rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button->SetImage(views::CustomButton::BS_PUSHED,
                         rb.GetBitmapNamed(IDR_CLOSE_BAR_P));

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  layout->SetInsets(kLayoutTopInset,
                    kLayoutLeftInset,
                    kLayoutBottomInset,
                    kLayoutRightInset);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(label1);
  layout->AddView(close_button, 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::LEADING);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 0);
  layout->AddView(label2);
}

gfx::Point FirstRunBubble::GetAnchorPoint() {
  // Compensate for padding in anchor.
  return BubbleDelegateView::GetAnchorPoint().Add(
      gfx::Point(0, anchor_view() ? kAnchorVerticalOffset : 0));
}

FirstRunBubble::FirstRunBubble(
    Profile* profile,
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    FirstRun::BubbleType bubble_type)
    : views::BubbleDelegateView(anchor_view,
                                arrow_location,
                                SK_ColorWHITE),
      profile_(profile),
      bubble_type_(bubble_type) {
}

FirstRunBubble::~FirstRunBubble() {
}

void FirstRunBubble::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  if (bubble_type_ == FirstRun::OEM_BUBBLE) {
    UserMetrics::RecordAction(
        UserMetricsAction("FirstRunOEMBubbleView_Clicked"));
  }
  GetWidget()->Close();
}
