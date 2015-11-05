// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/non_client_frame_view_impl.h"

#include "components/mus/public/cpp/window.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

NonClientFrameViewImpl::NonClientFrameViewImpl(mus::Window* window)
    : window_(window) {
  window_->AddObserver(this);
}

NonClientFrameViewImpl::~NonClientFrameViewImpl() {
  if (window_)
    window_->RemoveObserver(this);
}

gfx::Rect NonClientFrameViewImpl::GetBoundsForClientView() const {
  gfx::Rect result(GetLocalBounds());
  result.Inset(window_->client_area());
  return result;
}

void NonClientFrameViewImpl::OnPaint(gfx::Canvas* canvas) {
  canvas->Save();
  CustomFrameView::OnPaint(canvas);
  canvas->Restore();

  // The client app draws the client area. Make ours totally transparent so
  // we only see the client apps client area.
  canvas->FillRect(GetBoundsForClientView(), SK_ColorBLACK,
                   SkXfermode::kSrc_Mode);
}

void NonClientFrameViewImpl::PaintChildren(const ui::PaintContext& context) {
  CustomFrameView::PaintChildren(context);

  // The client app draws the client area. Make ours totally transparent so
  // we only see the client apps client area.
  ui::PaintRecorder recorder(context, size(), &paint_cache_);
  recorder.canvas()->FillRect(GetBoundsForClientView(), SK_ColorBLACK,
                              SkXfermode::kSrc_Mode);
}

void NonClientFrameViewImpl::OnWindowClientAreaChanged(
    mus::Window* window,
    const gfx::Insets& old_client_area) {
  Layout();
  SchedulePaint();
}

void NonClientFrameViewImpl::OnWindowDestroyed(mus::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}
