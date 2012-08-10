// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// The number of milliseconds the bubble should stay on the screen if it will
// close automatically.
const int kBubbleCloseDelay = 1500;

// The number of pixels between the separator and the button.
const int kSeparatorButtonSpacing = 2;

// How many pixels larger the percentage label font should be, compared to the
// default font.
const int kPercentageFontIncrease = 5;

}  // namespace

// static
ZoomBubbleView* ZoomBubbleView::zoom_bubble_ = NULL;

// static
void ZoomBubbleView::ShowBubble(views::View* anchor_view,
                                TabContents* tab_contents,
                                bool auto_close) {
  // If the bubble is already showing in this window and its |auto_close_| value
  // is equal to |auto_close|, the bubble can be reused and only the label text
  // needs to be updated.
  if (zoom_bubble_ &&
      zoom_bubble_->anchor_view() == anchor_view &&
      zoom_bubble_->auto_close_ == auto_close) {
    zoom_bubble_->Refresh();
  } else {
    // If the bubble is already showing but its |auto_close_| value is not equal
    // to |auto_close|, the bubble's focus properties must change, so the
    // current bubble must be closed and a new one created.
    if (zoom_bubble_)
      zoom_bubble_->Close();

    zoom_bubble_ = new ZoomBubbleView(anchor_view, tab_contents, auto_close);
    views::BubbleDelegateView::CreateBubble(zoom_bubble_);
    zoom_bubble_->Show();
  }
}

// static
void ZoomBubbleView::CloseBubble() {
  if (zoom_bubble_)
    zoom_bubble_->Close();
}

// static
bool ZoomBubbleView::IsShowing() {
  return zoom_bubble_ != NULL;
}

ZoomBubbleView::ZoomBubbleView(views::View* anchor_view,
                               TabContents* tab_contents,
                               bool auto_close)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      label_(NULL),
      tab_contents_(tab_contents),
      auto_close_(auto_close) {
  set_use_focusless(auto_close);
  set_notify_enter_exit_on_child(true);
}

ZoomBubbleView::~ZoomBubbleView() {
}

void ZoomBubbleView::Refresh() {
  int zoom_percent = tab_contents_->zoom_controller()->zoom_percent();
  label_->SetText(
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent));

  if (auto_close_) {
    // If the bubble should be closed automatically, reset the timer so that
    // it will show for the full amount of time instead of only what remained
    // from the previous time.
    timer_.Reset();
  }
}

void ZoomBubbleView::Close() {
  GetWidget()->Close();
}

void ZoomBubbleView::StartTimerIfNecessary() {
  if (auto_close_) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBubbleCloseDelay),
        this,
        &ZoomBubbleView::Close);
  }
}

void ZoomBubbleView::StopTimer() {
  timer_.Stop();
}

void ZoomBubbleView::OnMouseEntered(const views::MouseEvent& event) {
  StopTimer();
}

void ZoomBubbleView::OnMouseExited(const views::MouseEvent& event) {
  StartTimerIfNecessary();
}

void ZoomBubbleView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  chrome_page_zoom::Zoom(tab_contents_->web_contents(),
                         content::PAGE_ZOOM_RESET);
}

void ZoomBubbleView::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
      0, 0, views::kRelatedControlVerticalSpacing));

  int zoom_percent = tab_contents_->zoom_controller()->zoom_percent();
  label_ = new views::Label(
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent));
  gfx::Font font = label_->font().DeriveFont(kPercentageFontIncrease);
  label_->SetFont(font);
  AddChildView(label_);

  AddChildView(new views::Separator());

  views::TextButton* set_default_button = new views::TextButton(
      this, l10n_util::GetStringUTF16(IDS_ZOOM_SET_DEFAULT));
  set_default_button->set_alignment(views::TextButtonBase::ALIGN_CENTER);
  AddChildView(set_default_button);

  StartTimerIfNecessary();
}

gfx::Rect ZoomBubbleView::GetAnchorRect() {
  // Compensate for some built-in padding in the zoom image.
  gfx::Rect rect(BubbleDelegateView::GetAnchorRect());
  rect.Inset(0, anchor_view() ? 5 : 0);
  return rect;
}

void ZoomBubbleView::WindowClosing() {
  DCHECK(zoom_bubble_ == this);
  zoom_bubble_ = NULL;
}
