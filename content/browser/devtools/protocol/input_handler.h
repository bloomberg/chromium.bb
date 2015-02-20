// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

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
  void SetClient(scoped_ptr<DevToolsProtocolClient> client);

  Response EmulateTouchFromMouseEvent(const std::string& type,
                                      int x,
                                      int y,
                                      double timestamp,
                                      const std::string& button,
                                      double* delta_x,
                                      double* delta_y,
                                      int* modifiers,
                                      int* click_count);

  Response SynthesizePinchGesture(DevToolsCommandId command_id,
                                  int x,
                                  int y,
                                  double scale_factor,
                                  const int* relative_speed,
                                  const std::string* gesture_source_type);

  Response SynthesizeScrollGesture(DevToolsCommandId command_id,
                                   int x,
                                   int y,
                                   const int* x_distance,
                                   const int* y_distance,
                                   const int* x_overscroll,
                                   const int* y_overscroll,
                                   const bool* prevent_fling,
                                   const int* speed,
                                   const std::string* gesture_source_type);

  Response SynthesizeTapGesture(DevToolsCommandId command_id,
                                int x,
                                int y,
                                const int* duration,
                                const int* tap_count,
                                const std::string* gesture_source_type);

 private:
  RenderViewHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace input
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
