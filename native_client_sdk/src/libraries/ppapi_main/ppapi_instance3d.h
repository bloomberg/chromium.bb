// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_MAIN_PPAPI_INSTANCE3D_H_
#define PPAPI_MAIN_PPAPI_INSTANCE3D_H_

#include <map>

#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/utility/completion_callback_factory.h"

#include "ppapi_main/ppapi_instance.h"


class PPAPIInstance3D : public PPAPIInstance {
 public:
  PPAPIInstance3D(PP_Instance instance, const char *args[]);
  virtual ~PPAPIInstance3D();

  // Called during initialization
  virtual bool Init(uint32_t arg, const char* argn[], const char* argv[]);

  // Called whenever the in-browser window changes size.
  virtual void DidChangeView(const pp::View& view);

  // Called when we need to rebuild the context
  virtual void BuildContext(int32_t result, const pp::Size& new_size);

  // Called whenever a swap takes place
  virtual void Swapped(int result);

  // Toggle in and out of Fullscreen mode
  virtual bool ToggleFullscreen();

  static PPAPIInstance3D* GetInstance3D();

 protected:
  static void *RenderLoop(void *this_ptr);

  pp::CompletionCallbackFactory<PPAPIInstance3D> callback_factory_;
  pp::Fullscreen fullscreen_;
  pp::Graphics3D device_context_;
  pp::Size size_;
  pp::MessageLoop render_loop_;
  bool is_context_bound_;
  bool was_fullscreen_;
  bool mouse_locked_;
  bool main_thread_3d_;
};


#endif  // PPAPI_MAIN_PPAPI_INSTANCE3D_H_
