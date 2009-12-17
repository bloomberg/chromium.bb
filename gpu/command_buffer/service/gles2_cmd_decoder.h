// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_

#include <build/build_config.h>
#if defined(OS_LINUX)
#include "gpu/command_buffer/service/x_utils.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif
#include "gpu/command_buffer/service/common_decoder.h"

namespace gpu {
namespace gles2 {

// This class implements the AsyncAPIInterface interface, decoding GLES2
// commands and calling GL.
class GLES2Decoder : public CommonDecoder {
 public:
  typedef parse_error::ParseError ParseError;

  // Creates a decoder.
  static GLES2Decoder* Create();

  virtual ~GLES2Decoder() {
  }

  bool debug() const {
    return debug_;
  }

  void set_debug(bool debug) {
    debug_ = debug;
  }

#if defined(OS_LINUX)
  void set_window_wrapper(XWindowWrapper *window) {
    window_ = window;
  }
  XWindowWrapper* window() const {
    return window_;
  }
#elif defined(OS_WIN)
  void set_hwnd(HWND hwnd) {
    hwnd_ = hwnd;
  }

  HWND hwnd() const {
    return hwnd_;
  }
#endif

  // Initializes the graphics context.
  // Returns:
  //   true if successful.
  virtual bool Initialize() = 0;

  // Destroys the graphics context.
  virtual void Destroy() = 0;

 protected:
  GLES2Decoder();

 private:
  bool debug_;

#if defined(OS_LINUX)
  XWindowWrapper *window_;
#elif defined(OS_WIN)
  // Handle to the GL device.
  HWND hwnd_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
