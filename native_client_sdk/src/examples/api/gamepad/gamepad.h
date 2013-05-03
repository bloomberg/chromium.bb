// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_GAMEPAD_GAMEPAD_H_
#define EXAMPLES_GAMEPAD_GAMEPAD_H_

#include <map>
#include <vector>
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

namespace gamepad {

// The Instance class.  One of these exists for each instance of your NaCl
// module on the web page.  The browser will ask the Module object to create
// a new Instance for each occurrence of the <embed> tag that has these
// attributes:
//     type="application/x-nacl"
//     nacl="pi_generator.nmf"
class Gamepad : public pp::Instance {
 public:
  explicit Gamepad(PP_Instance instance);
  virtual ~Gamepad();

  // Update the graphics context to the new size, and regenerate |pixel_buffer_|
  // to fit the new size as well.
  virtual void DidChangeView(const pp::View& view);

  // Flushes its contents of |pixel_buffer_| to the 2D graphics context.
  void Paint();

  bool quit() const { return quit_; }

  int width() const {
    return pixel_buffer_ ? pixel_buffer_->size().width() : 0;
  }
  int height() const {
    return pixel_buffer_ ? pixel_buffer_->size().height() : 0;
  }

  // Indicate whether a flush is pending.  This can only be called from the
  // main thread; it is not thread safe.
  bool flush_pending() const { return flush_pending_; }
  void set_flush_pending(bool flag) { flush_pending_ = flag; }

 private:
  // Create and initialize the 2D context used for drawing.
  void CreateContext(const pp::Size& size);
  // Destroy the 2D drawing context.
  void DestroyContext();
  // Push the pixels to the browser, then attempt to flush the 2D context.  If
  // there is a pending flush on the 2D context, then update the pixels only
  // and do not flush.
  void FlushPixelBuffer();

  bool IsContextValid() const { return graphics_2d_context_ != NULL; }

  pp::Graphics2D* graphics_2d_context_;
  pp::ImageData* pixel_buffer_;
  const PPB_Gamepad* gamepad_;
  bool flush_pending_;
  bool quit_;
};

}  // namespace gamepad

#endif  // EXAMPLES_GAMEPAD_GAMEPAD_H_
