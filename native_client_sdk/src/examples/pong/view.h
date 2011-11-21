// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_PONG_VIEW_H_
#define EXAMPLES_PONG_VIEW_H_

#include "ppapi/cpp/rect.h"

namespace pp {
class Graphics2D;
class ImageData;
class Instance;
class KeyboardInputEvent;
class Rect;
class Size;
}  // namespace pp

namespace pong {

class View {
 public:
  explicit View(pp::Instance* instance);
  ~View();

  const uint32_t& last_key_code() const {
    return last_key_code_;
  }
  void set_left_paddle_rect(const pp::Rect& left_paddle_rect) {
    left_paddle_rect_ = left_paddle_rect;
  }
  void set_right_paddle_rect(const pp::Rect& right_paddle_rect) {
    right_paddle_rect_ = right_paddle_rect;
  }
  void set_ball_rect(const pp::Rect& ball_rect) {
    ball_rect_ = ball_rect;
  }
  void set_flush_pending(bool flush_pending) {
    flush_pending_ = flush_pending;
  }
  pp::Size GetSize() const;
  bool KeyDown(const pp::KeyboardInputEvent& key);
  bool KeyUp(const pp::KeyboardInputEvent& key);
  void Draw();
  void UpdateView(const pp::Rect& position,
                  const pp::Rect& clip,
                  pp::Instance* instance);

 private:
  pp::Instance* const instance_;  // weak
  // Create and initialize the 2D context used for drawing.
  void CreateContext(const pp::Size& size, pp::Instance* instance);
  // Destroy the 2D drawing context.
  void DestroyContext();
  // Push the pixels to the browser, then attempt to flush the 2D context.  If
  // there is a pending flush on the 2D context, then update the pixels only
  // and do not flush.
  void FlushPixelBuffer();
  bool IsContextValid() const;

  uint32_t last_key_code_;
  // Geometry for drawing
  pp::Rect left_paddle_rect_;
  pp::Rect right_paddle_rect_;
  pp::Rect ball_rect_;
  // Drawing stuff
  bool flush_pending_;
  pp::Graphics2D* graphics_2d_context_;
  pp::ImageData* pixel_buffer_;
};


}  // namespace pong

#endif  // EXAMPLES_PONG_VIEW_H_
