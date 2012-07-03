// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The number of milliseconds the bubble should stay on the screen for if it
// will automatically close.
const int kBubbleCloseDelay = 400;

}

// static
ZoomBubbleView* ZoomBubbleView::zoom_bubble_ = NULL;

// static
void ZoomBubbleView::ShowBubble(views::View* anchor_view,
                                int zoom_percent,
                                bool auto_close) {
  // If the bubble is already showing in this window and its |auto_close_| value
  // is equal to |auto_close|, the bubble can be reused and only the label text
  // needs to be updated.
  if (zoom_bubble_ &&
      zoom_bubble_->anchor_view() == anchor_view &&
      zoom_bubble_->auto_close_ == auto_close) {
    zoom_bubble_->label_->SetText(
        l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, zoom_percent));

    if (auto_close) {
      // If the bubble should be closed automatically, reset the timer so that
      // it will show for the full amount of time instead of only what remained
      // from the previous time.
      zoom_bubble_->timer_.Reset();
    }
  } else {
    // If the bubble is already showing but its |auto_close_| value is not equal
    // to |auto_close|, the bubble's focus properties must change, so the
    // current bubble must be closed and a new one created.
    if (zoom_bubble_)
      zoom_bubble_->Close();

    zoom_bubble_ = new ZoomBubbleView(anchor_view, zoom_percent, auto_close);
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

void ZoomBubbleView::WindowClosing() {
  DCHECK(zoom_bubble_ == this);
  zoom_bubble_ = NULL;
}

void ZoomBubbleView::Init() {
  SetLayoutManager(new views::FillLayout());
  label_ = new views::Label(
      l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, zoom_percent_));
  AddChildView(label_);

  if (auto_close_) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBubbleCloseDelay),
        this,
        &ZoomBubbleView::Close);
  }
}

ZoomBubbleView::ZoomBubbleView(views::View* anchor_view,
                               int zoom_percent,
                               bool auto_close)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      zoom_percent_(zoom_percent),
      auto_close_(auto_close) {
  set_use_focusless(auto_close);
}

ZoomBubbleView::~ZoomBubbleView() {
}

void ZoomBubbleView::Close() {
  GetWidget()->Close();
}
