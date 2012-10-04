// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_PONG_VIEW_H_
#define EXAMPLES_PONG_VIEW_H_

#include "ppapi/cpp/rect.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "pong_model.h"

namespace pp {
class Graphics2D;
class ImageData;
class Instance;
class Size;
class View;
}  // namespace pp

class PongView {
 public:
  explicit PongView(PongModel* model);
  ~PongView();

  bool DidChangeView(pp::Instance* instance, const pp::View& view,
                     bool first_view_change);
  pp::Size GetSize() const;
  void StartDrawLoop();

 private:
  void DrawCallback(int32_t result);
  void DrawRect(const pp::Rect& rect, uint32_t color);
  void DrawBall(const BallModel& ball);
  void DrawPaddle(const PaddleModel& paddle);
  void EraseBall(const BallModel& ball);
  void ErasePaddle(const PaddleModel& paddle);
  void PaintRectToGraphics2D(const pp::Rect& rect);
  void PaintRectToGraphics2D(const pp::Rect& old_rect, const pp::Rect& rect);

  pp::CompletionCallbackFactory<PongView> factory_;
  PongModel* model_;  // Weak pointer.
  pp::Graphics2D* graphics_2d_;
  pp::ImageData* pixel_buffer_;

  bool needs_erase_;
  PaddleModel old_left_paddle_;
  PaddleModel old_right_paddle_;
  BallModel old_ball_;
};

#endif  // EXAMPLES_PONG_VIEW_H_
