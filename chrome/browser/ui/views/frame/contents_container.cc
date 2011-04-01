// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/slide_animation.h"
#include "views/background.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

// Min/max opacity of the overlay.
static const int kMinOpacity = 0;
static const int kMaxOpacity = 192;

// View used to track when the overlay widget is destroyed. If the
// ContentsContainer is still valid when the destructor is invoked this invokes
// |OverlayViewDestroyed| on the ContentsContainer.
class ContentsContainer::OverlayContentView : public views::View {
 public:
  explicit OverlayContentView(ContentsContainer* container)
      : container_(container) {
  }
  ~OverlayContentView() {
    if (container_)
      container_->OverlayViewDestroyed();
  }

  void Detach() {
    container_ = NULL;
  }

 private:
  ContentsContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(OverlayContentView);
};

ContentsContainer::ContentsContainer(views::View* active)
    : active_(active),
      preview_(NULL),
      preview_tab_contents_(NULL),
      active_overlay_(NULL),
      overlay_view_(NULL),
      active_top_margin_(0) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
  // We don't need to explicitly delete active_overlay_ as it'll be deleted by
  // virtue of being a child window.
  if (overlay_view_)
    overlay_view_->Detach();
}

void ContentsContainer::MakePreviewContentsActiveContents() {
  DCHECK(preview_);
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
  if (active_overlay_ || !ui::Animation::ShouldRenderRichAnimation())
    return;

#if !defined(OS_WIN)
  // TODO: fix this. I'm disabling as z-order isn't right on linux so that
  // overlay ends up obscuring the omnibox.
  return;
#endif

  overlay_animation_.reset(new ui::SlideAnimation(this));
  overlay_animation_->SetDuration(300);
  overlay_animation_->SetSlideDuration(300);
  overlay_animation_->Show();

  CreateOverlay(kMinOpacity);
}

void ContentsContainer::ShowFade() {
  if (active_overlay_ || !ui::Animation::ShouldRenderRichAnimation())
    return;

  CreateOverlay(kMaxOpacity);
}

void ContentsContainer::RemoveFade() {
  overlay_animation_.reset();
  if (active_overlay_) {
    overlay_view_->Detach();
    overlay_view_ = NULL;
    active_overlay_->Close();
    active_overlay_ = NULL;
  }
}

void ContentsContainer::AnimationProgressed(const ui::Animation* animation) {
  active_overlay_->SetOpacity(
      ui::Tween::ValueBetween(animation->GetCurrentValue(), kMinOpacity,
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

void ContentsContainer::CreateOverlay(int initial_opacity) {
  DCHECK(!active_overlay_);
  views::Widget::CreateParams params(views::Widget::CreateParams::TYPE_POPUP);
  params.transparent = true;
  params.accept_events = false;
  active_overlay_ = views::Widget::CreateWidget(params);
  active_overlay_->SetOpacity(initial_opacity);
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(active_, &screen_origin);
  gfx::Rect overlay_bounds(screen_origin, active_->size());
  active_overlay_->Init(active_->GetWidget()->GetNativeView(), overlay_bounds);
  overlay_view_ = new OverlayContentView(this);
  overlay_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  active_overlay_->SetContentsView(overlay_view_);
  active_overlay_->Show();
  active_overlay_->MoveAboveWidget(active_->GetWidget());
}

void ContentsContainer::OverlayViewDestroyed() {
  active_overlay_ = NULL;
  overlay_view_ = NULL;
}
