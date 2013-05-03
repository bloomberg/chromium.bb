// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pong_view.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/point.h"

namespace {

const uint32_t kBlack = 0xff000000;
const uint32_t kWhite = 0xffffffff;

pp::Rect ClipRect(const pp::Rect& rect, const pp::Rect& clip) {
  return clip.Intersect(rect);
}

}  // namespace

PongView::PongView(PongModel* model)
    : factory_(this),
      model_(model),
      graphics_2d_(NULL),
      pixel_buffer_(NULL),
      needs_erase_(false) {}

PongView::~PongView() {
  delete graphics_2d_;
  delete pixel_buffer_;
}

bool PongView::DidChangeView(pp::Instance* instance,
                             const pp::View& view,
                             bool first_view_change) {
  pp::Size old_size = GetSize();
  pp::Size new_size = view.GetRect().size();
  if (old_size == new_size)
    return true;

  delete graphics_2d_;
  graphics_2d_ =
      new pp::Graphics2D(instance, new_size, true);  // is_always_opaque
  if (!instance->BindGraphics(*graphics_2d_)) {
    delete graphics_2d_;
    graphics_2d_ = NULL;
    return false;
  }

  // Create a new pixel buffer, the same size as the graphics context. We'll
  // write to this buffer directly, and copy regions of it to the graphics
  // context's backing store to draw to the screen.
  delete pixel_buffer_;
  pixel_buffer_ = new pp::ImageData(instance,
                                    PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                    new_size,
                                    true);  // init_to_zero

  needs_erase_ = false;

  if (first_view_change)
    DrawCallback(0);  // Start the draw loop.

  return true;
}

pp::Size PongView::GetSize() const {
  return graphics_2d_ ? graphics_2d_->size() : pp::Size();
}

void PongView::DrawCallback(int32_t result) {
  assert(graphics_2d_);
  assert(pixel_buffer_);

  if (needs_erase_) {
    EraseBall(old_ball_);
    ErasePaddle(old_left_paddle_);
    ErasePaddle(old_right_paddle_);
  }

  // Draw the image to pixel_buffer_.
  DrawBall(model_->ball());
  DrawPaddle(model_->left_paddle());
  DrawPaddle(model_->right_paddle());

  // Paint to the graphics context's backing store.
  if (needs_erase_) {
    // Update the regions for the old positions of the ball and both paddles,
    // as well as the new positions.
    PaintRectToGraphics2D(old_ball_.rect, model_->ball().rect);
    PaintRectToGraphics2D(old_left_paddle_.rect, model_->left_paddle().rect);
    PaintRectToGraphics2D(old_right_paddle_.rect, model_->right_paddle().rect);
  } else {
    // Only update the regions for the new positions. The old positions are
    // invalid.
    PaintRectToGraphics2D(model_->ball().rect);
    PaintRectToGraphics2D(model_->left_paddle().rect);
    PaintRectToGraphics2D(model_->right_paddle().rect);
  }

  // Save the locations of the ball and paddles to erase next time.
  old_ball_ = model_->ball();
  old_left_paddle_ = model_->left_paddle();
  old_right_paddle_ = model_->right_paddle();
  needs_erase_ = true;

  // Graphics2D::Flush writes all paints to the graphics context's backing
  // store. When it is finished, it calls the callback. By hooking our draw
  // function to the Flush callback, we will be able to draw as quickly as
  // possible.
  graphics_2d_->Flush(factory_.NewCallback(&PongView::DrawCallback));
}

void PongView::DrawRect(const pp::Rect& rect, uint32_t color) {
  assert(pixel_buffer_);
  uint32_t* pixel_data = static_cast<uint32_t*>(pixel_buffer_->data());
  assert(pixel_data);

  int width = pixel_buffer_->size().width();
  int height = pixel_buffer_->size().height();

  // We shouldn't be drawing rectangles outside of the pixel buffer bounds,
  // but just in case, clip the rectangle first.
  pp::Rect clipped_rect = ClipRect(rect, pp::Rect(pixel_buffer_->size()));
  int offset = clipped_rect.y() * width + clipped_rect.x();

  for (int y = 0; y < clipped_rect.height(); ++y) {
    for (int x = 0; x < clipped_rect.width(); ++x) {
      // As a sanity check, make sure that we aren't about to stomp memory.
      assert(offset >= 0 && offset < width * height);
      pixel_data[offset++] = color;
    }
    offset += width - clipped_rect.width();
  }
}

void PongView::DrawBall(const BallModel& ball) {
  assert(pixel_buffer_);
  uint32_t* pixel_data = static_cast<uint32_t*>(pixel_buffer_->data());
  assert(pixel_data);

  int width = pixel_buffer_->size().width();
  int height = pixel_buffer_->size().height();

  // We shouldn't be drawing outside of the pixel buffer bounds, but just in
  // case, clip the rectangle first.
  pp::Rect clipped_rect = ClipRect(ball.rect, pp::Rect(pixel_buffer_->size()));

  int radius2 = (ball.rect.width() / 2) * (ball.rect.width() / 2);
  pp::Point ball_center = ball.rect.CenterPoint() - clipped_rect.point();

  int offset = clipped_rect.y() * width + clipped_rect.x();

  for (int y = 0; y < clipped_rect.height(); ++y) {
    for (int x = 0; x < clipped_rect.width(); ++x) {
      int distance_x = x - ball_center.x();
      int distance_y = y - ball_center.y();
      int distance2 = distance_x * distance_x + distance_y * distance_y;

      // As a sanity check, make sure that we aren't about to stomp memory.
      assert(offset >= 0 && offset < width * height);
      // Common optimization here: compare the distance from the center of the
      // ball to the current pixel squared versus the radius squared. This way
      // we avoid the sqrt.
      pixel_data[offset++] = distance2 <= radius2 ? kWhite : kBlack;
    }

    offset += width - clipped_rect.width();
  }
}

void PongView::DrawPaddle(const PaddleModel& paddle) {
  DrawRect(paddle.rect, kWhite);
}

void PongView::EraseBall(const BallModel& ball) { DrawRect(ball.rect, kBlack); }

void PongView::ErasePaddle(const PaddleModel& paddle) {
  DrawRect(paddle.rect, kBlack);
}

void PongView::PaintRectToGraphics2D(const pp::Rect& rect) {
  const pp::Point top_left(0, 0);
  graphics_2d_->PaintImageData(*pixel_buffer_, top_left, rect);
}

void PongView::PaintRectToGraphics2D(const pp::Rect& old_rect,
                                     const pp::Rect& rect) {
  // Try a little optimization here: since the paddles and balls don't move
  // very much per frame, they often will overlap their old bounding boxes. When
  // they do, update the union of the two boxes to save some time.
  if (old_rect.Intersects(rect)) {
    PaintRectToGraphics2D(old_rect.Union(rect));
  } else {
    PaintRectToGraphics2D(old_rect);
    PaintRectToGraphics2D(rect);
  }
}
