// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_NON_CLIENT_FRAME_VIEW_IMPL_H_
#define COMPONENTS_MUS_EXAMPLE_WM_NON_CLIENT_FRAME_VIEW_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_observer.h"
#include "ui/compositor/paint_cache.h"
#include "ui/views/window/custom_frame_view.h"

class MoveLoop;

class NonClientFrameViewImpl : public views::CustomFrameView,
                               public mus::WindowObserver {
 public:
  explicit NonClientFrameViewImpl(mus::Window* window);
  ~NonClientFrameViewImpl() override;

 private:
  // CustomFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const ui::PaintContext& context) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;

  // mus::WindowObserver:
  void OnWindowClientAreaChanged(mus::Window* window,
                                 const gfx::Insets& old_client_area) override;
  void OnWindowDestroyed(mus::Window* window) override;

 private:
  bool StartMoveLoopIfNecessary(const ui::Event& event);
  void ContinueMove(const ui::Event& event);
  void StopMove();

  mus::Window* window_;
  ui::PaintCache paint_cache_;
  scoped_ptr<MoveLoop> move_loop_;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewImpl);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_NON_CLIENT_FRAME_VIEW_IMPL_H_
