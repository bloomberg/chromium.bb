// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"

#include "ppapi/utility/completion_callback_factory.h"

#include "ppapi_main/ppapi_instance2d.h"
#include "ppapi_main/ppapi_main.h"


void* PPAPI_CreateInstance2D(PP_Instance inst, const char *args[]) {
  return static_cast<void*>(new PPAPIInstance2D(inst, args));
}


PPAPIInstance2D* PPAPIInstance2D::GetInstance2D() {
  return static_cast<PPAPIInstance2D*>(PPAPI_GetInstanceObject());
}


void PPAPIInstance2D::Flushed(int result) {
  if (result != 0) {
    printf("Flush result=%d.\n", result);
  }

  if (is_context_bound_) {
    Render(device_context_.pp_resource(), size_.width(), size_.height());

    int result;
    result = device_context_.Flush(callback_factory_.NewCallback(
                                         &PPAPIInstance::Flushed));
    if (result == PP_OK_COMPLETIONPENDING) return;
    printf("Failed Flush with %d.\n", result);
  }

  // Failed to draw, so add a callback for the future.  This could
  // an application choice or the browser dealing with an event such as
  // fullscreen toggle.  We add a delay of 100ms (to prevent burnning CPU
  // in cases where the context will not be available for a while.
  pp::MessageLoop::GetCurrent().PostWork(callback_factory_.NewCallback(
                                         &PPAPIInstance::Flushed), 100);
}

void PPAPIInstance2D::BuildContext(int32_t result, const pp::Size& new_size) {
  printf("Building context.\n");

  size_ = new_size;
  device_context_ = pp::Graphics2D(this, size_ ,true);
  printf("Got Context!\n");

  is_context_bound_ = BindGraphics(device_context_);
  printf("Context is bound=%d\n", is_context_bound_);

  if (is_context_bound_) {
    PPAPIBuildContext(size_.width(), size_.height());
    device_context_.Flush(callback_factory_.NewCallback(
                          &PPAPIInstance::Flushed));
  } else {
    fprintf(stderr, "Failed to bind context for %dx%d.\n", size_.width(),
            size_.height());
  }
}

// The default implementation calls the 'C' render function.
void PPAPIInstance2D::Render(PP_Resource ctx, uint32_t width,
                             uint32_t height) {
  PPAPIRender(ctx, width, height);
}

PPAPIInstance2D::PPAPIInstance2D(PP_Instance instance, const char *args[])
    : PPAPIInstance(instance, args) {
}


PPAPIInstance2D::~PPAPIInstance2D() {
  if (is_context_bound_) {
    is_context_bound_ = false;
    // Cleanup code?
  }
}
