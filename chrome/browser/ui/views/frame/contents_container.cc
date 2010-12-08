// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/contents_container.h"

#include "app/slide_animation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/background.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

// Min/max opacity of the overlay.
static const int kMinOpacity = 0;
static const int kMaxOpacity = 192;

ContentsContainer::ContentsContainer(views::View* active)
    : active_(active),
      preview_(NULL),
      preview_tab_contents_(NULL),
      active_overlay_(NULL),
      active_top_margin_(0) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
  // We don't need to explicitly delete active_overlay_ as it'll be deleted by
  // virtue of being a child window.
}

void ContentsContainer::MakePreviewContentsActiveContents() {
  RemoveFade();

  active_ = preview_;
  preview_ = NULL;
  Layout();
}

void ContentsContainer::SetPreview(views::View* preview,
                                   TabContents* preview_tab_contents) {
  if (preview == preview_)
    return;

  if (preview_)
    RemoveChildView(preview_);
  preview_ = preview;
  preview_tab_contents_ = preview_tab_contents;
  if (preview_)
    AddChildView(preview_);

  Layout();
}

void ContentsContainer::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return;

  active_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
}

gfx::Rect ContentsContainer::GetPreviewBounds() {
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

void ContentsContainer::FadeActiveContents() {
  if (active_overlay_ || !Animation::ShouldRenderRichAnimation())
    return;

#if !defined(OS_WIN)
  // TODO: fix this. I'm disabling as z-order isn't right on linux so that
  // overlay ends up obscuring the omnibox.
  return;
#endif

  overlay_animation_.reset(new SlideAnimation(this));
  overlay_animation_->SetDuration(300);
  overlay_animation_->SetSlideDuration(300);
  overlay_animation_->Show();

  active_overlay_ = views::Widget::CreatePopupWidget(views::Widget::Transparent,
                                              views::Widget::NotAcceptEvents,
                                              views::Widget::DeleteOnDestroy,
                                              views::Widget::MirrorOriginInRTL);
  active_overlay_->SetOpacity(0);
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(active_, &screen_origin);
  gfx::Rect overlay_bounds(screen_origin, active_->size());
  active_overlay_->Init(active_->GetWidget()->GetNativeView(), overlay_bounds);
  views::View* content_view = new views::View();
  content_view->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  active_overlay_->SetContentsView(content_view);
  active_overlay_->Show();
  active_overlay_->MoveAbove(active_->GetWidget());
}

void ContentsContainer::RemoveFade() {
  overlay_animation_.reset();
  if (active_overlay_) {
    active_overlay_->Close();
    active_overlay_ = NULL;
  }
}

void ContentsContainer::AnimationProgressed(const Animation* animation) {
  active_overlay_->SetOpacity(
      Tween::ValueBetween(animation->GetCurrentValue(), kMinOpacity,
                          kMaxOpacity));
  active_overlay_->GetRootView()->SchedulePaint();
}

void ContentsContainer::Layout() {
  // The active view always gets the full bounds.
  active_->SetBounds(0, active_top_margin_, width(),
                     std::max(0, height() - active_top_margin_));

  if (preview_)
    preview_->SetBounds(0, 0, width(), height());

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}
