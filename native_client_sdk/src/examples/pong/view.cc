// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/pong/view.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var.h"

// Input event key codes.  PPAPI uses Windows Virtual key codes.
const uint32_t kSpaceBar = 0x20;
const uint32_t kUpArrow = 0x26;
const uint32_t kDownArrow = 0x28;

namespace {

const uint32_t kOpaqueColorMask = 0xff000000;  // Opaque pixels.
const uint32_t kWhiteMask = 0xffffff;

// This is called by the browser when the 2D context has been flushed to the
// browser window.
void FlushCallback(void* data, int32_t result) {
  static_cast<pong::View*>(data)->set_flush_pending(false);
}

}  // namespace

namespace pong {
View::View(pp::Instance* instance)
    : instance_(instance), last_key_code_(0x0), flush_pending_(false),
    graphics_2d_context_(NULL), pixel_buffer_(NULL) {}

View::~View() {
  DestroyContext();
  delete pixel_buffer_;
}

pp::Size View::GetSize() const {
  pp::Size size;
  if (graphics_2d_context_) {
    size.SetSize(graphics_2d_context_->size().width(),
                  graphics_2d_context_->size().height());
  }
  return size;
}

bool View::KeyDown(const pp::KeyboardInputEvent& key) {
  last_key_code_ = key.GetKeyCode();
  if (last_key_code_ == kSpaceBar || last_key_code_ == kUpArrow ||
      last_key_code_ == kDownArrow)
    return true;
  return false;
}

bool View::KeyUp(const pp::KeyboardInputEvent& key) {
  if (last_key_code_ == key.GetKeyCode()) {
    last_key_code_ = 0x0;  // Indicates key code is not set.
  }
  return false;
}

void View::Draw() {
  uint32_t* pixels = static_cast<uint32_t*>(pixel_buffer_->data());
  if (NULL == pixels)
    return;
  // Clear the buffer
  const int32_t height = pixel_buffer_->size().height();
  const int32_t width = pixel_buffer_->size().width();
  for (int32_t py = 0; py < height; ++py) {
    for (int32_t px = 0; px < width; ++px) {
      const int32_t pos = px + py * width;
      uint32_t color = kOpaqueColorMask;
      // Draw the paddles
      if (left_paddle_rect_.Contains(px, py) ||
          right_paddle_rect_.Contains(px, py)) {
        color |= kWhiteMask;
      } else {
        pp::Point center_point = ball_rect_.CenterPoint();
        float radius = ball_rect_.width() / 2;
        float distance_x = px - center_point.x();
        float distance_y = py - center_point.y();
        float distance =
            sqrt(distance_x * distance_x + distance_y * distance_y);
        // Draw the ball
        if (distance <= radius)
          color |= kWhiteMask;
      }
      pixels[pos] = color;
    }
  }

  FlushPixelBuffer();
}

void View::UpdateView(const pp::Rect& position,
                      const pp::Rect& clip,
                      pp::Instance* instance) {
  const int32_t width =
      pixel_buffer_ ? pixel_buffer_->size().width() : 0;
  const int32_t height =
      pixel_buffer_ ? pixel_buffer_->size().height() : 0;

  if (position.size().width() == width &&
      position.size().height() == height)
    return;  // Size didn't change, no need to update anything.

  // Create a new device context with the new size.
  DestroyContext();
  CreateContext(position.size(), instance);
  // Delete the old pixel buffer and create a new one.
  delete pixel_buffer_;
  pixel_buffer_ = NULL;
  if (graphics_2d_context_ != NULL) {
    pixel_buffer_ = new pp::ImageData(instance, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                      graphics_2d_context_->size(),
                                      false);
  }
}

void View::CreateContext(const pp::Size& size, pp::Instance* instance) {
  if (IsContextValid())
    return;
  graphics_2d_context_ = new pp::Graphics2D(instance, size,
                                            false);
  if (!instance->BindGraphics(*graphics_2d_context_)) {
    instance_->PostMessage(pp::Var("ERROR: Couldn't bind the device context"));
  }
}

void View::DestroyContext() {
  if (!IsContextValid())
    return;
  delete graphics_2d_context_;
  graphics_2d_context_ = NULL;
}

void View::FlushPixelBuffer() {
  if (!IsContextValid())
    return;
  // Note that the pixel lock is held while the buffer is copied into the
  // device context and then flushed.
  graphics_2d_context_->PaintImageData(*pixel_buffer_, pp::Point());
  if (flush_pending_)
    return;
  flush_pending_ = true;
  graphics_2d_context_->Flush(pp::CompletionCallback(&FlushCallback, this));
}

bool View::IsContextValid() const {
    return graphics_2d_context_ != NULL;
}

}  // namespace pong
