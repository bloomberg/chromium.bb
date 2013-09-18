// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_prompt_view.h"

#include "chrome/browser/ui/bookmarks/bookmark_prompt_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Horizontal padding of bookmark prompt.
const int kHorizontalPadding = 20;

}

// static
BookmarkPromptView* BookmarkPromptView::bookmark_bubble_ = NULL;

// static
void BookmarkPromptView::ShowPrompt(views::View* anchor_view,
                                    PrefService* prefs) {
  if (bookmark_bubble_)
    return;
  bookmark_bubble_ = new BookmarkPromptView(anchor_view, prefs);
  views::BubbleDelegateView::CreateBubble(bookmark_bubble_)->Show();
}

BookmarkPromptView::BookmarkPromptView(views::View* anchor_view,
                                       PrefService* prefs)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      dismiss_link_(NULL),
      prefs_(prefs) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
}

BookmarkPromptView::~BookmarkPromptView() {
  DCHECK_NE(this, bookmark_bubble_);
}

void BookmarkPromptView::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kHorizontalPadding, 0, 0));

  views::Label* label = new views::Label(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_PROMPT_MESSAGE));
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  label->SetFont(rb->GetFont(ui::ResourceBundle::MediumFont)
                   .DeriveFont(0, gfx::Font::BOLD));
  AddChildView(label);

  dismiss_link_ = new views::Link(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_PROMPT_DISMISS));
  dismiss_link_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  dismiss_link_->set_listener(this);
  AddChildView(dismiss_link_);
}

void BookmarkPromptView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, dismiss_link_);
  BookmarkPromptController::DisableBookmarkPrompt(prefs_);
  StartFade(false);
}

void BookmarkPromptView::WindowClosing() {
  bookmark_bubble_ = NULL;
  BookmarkPromptController::ClosingBookmarkPrompt();
}
