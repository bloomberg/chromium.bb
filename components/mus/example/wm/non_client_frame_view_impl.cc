// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/non_client_frame_view_impl.h"

#include "components/mus/example/wm/move_loop.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
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

bool NonClientFrameViewImpl::OnMousePressed(const ui::MouseEvent& event) {
  return StartMoveLoopIfNecessary(event);
}

bool NonClientFrameViewImpl::OnMouseDragged(const ui::MouseEvent& event) {
  ContinueMove(event);
  return move_loop_.get() != nullptr;
}

void NonClientFrameViewImpl::OnMouseReleased(const ui::MouseEvent& event) {
  ContinueMove(event);
}

void NonClientFrameViewImpl::OnMouseCaptureLost() {
  StopMove();
}

void NonClientFrameViewImpl::OnWindowClientAreaChanged(
    mus::Window* window,
    const gfx::Insets& old_client_area) {
  Layout();
  // NonClientView (our parent) positions the client view based on bounds from
  // us. We need to layout from parent to trigger a layout of the client view.
  if (parent())
    parent()->Layout();
  SchedulePaint();
}

void NonClientFrameViewImpl::OnWindowDestroyed(mus::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

bool NonClientFrameViewImpl::StartMoveLoopIfNecessary(const ui::Event& event) {
  if (move_loop_)
    return false;
  // TODO(sky): convert MoveLoop to take ui::Event.
  move_loop_ = MoveLoop::Create(window_, *mus::mojom::Event::From(event));
  return true;
}

void NonClientFrameViewImpl::ContinueMove(const ui::Event& event) {
  // TODO(sky): convert MoveLoop to take ui::Event.
  if (move_loop_ &&
      move_loop_->Move(*mus::mojom::Event::From(event)) == MoveLoop::DONE) {
    move_loop_.reset();
  }
}

void NonClientFrameViewImpl::StopMove() {
  move_loop_.reset();
}
