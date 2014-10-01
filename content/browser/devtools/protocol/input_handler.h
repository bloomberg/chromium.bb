// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"

namespace content {

class RenderViewHostImpl;

namespace devtools {
namespace input {

class InputHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  InputHandler();
  virtual ~InputHandler();

  void SetRenderViewHost(RenderViewHostImpl* host);

  Response EmulateTouchFromMouseEvent(const std::string& type,
                                      int x,
                                      int y,
                                      double timestamp,
                                      const std::string& button,
                                      double* delta_x,
                                      double* delta_y,
                                      int* modifiers,
                                      int* click_count);

 private:
  RenderViewHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace inpue
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
