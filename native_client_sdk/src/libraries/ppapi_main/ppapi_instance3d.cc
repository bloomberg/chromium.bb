// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "nacl_io/nacl_io.h"

#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"

#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"

#include "ppapi/utility/completion_callback_factory.h"

#include "ppapi_main/ppapi_instance.h"
#include "ppapi_main/ppapi_instance3d.h"
#include "ppapi_main/ppapi_main.h"


static const uint32_t kReturnKeyCode = 13;


void* PPAPI_CreateInstance3D(PP_Instance inst, const char *args[]) {
  return static_cast<void*>(new PPAPIInstance3D(inst, args));
}


int32_t *PPAPIGet3DAttribs(uint32_t width, uint32_t height) {
  static int32_t attribs[] = {
    PP_GRAPHICS3DATTRIB_WIDTH, 0,
    PP_GRAPHICS3DATTRIB_HEIGHT, 0,
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_NONE
  };

  attribs[1] = width;
  attribs[3] = height;

  printf("Building attribs for %dx%d.\n", width, height);
  return attribs;
}

void PPAPIBuildContext(uint32_t width, uint32_t height) {
  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  printf("Built Context %d, %d.\n", width, height);
}


PPAPIInstance3D* PPAPIInstance3D::GetInstance3D() {
  return static_cast<PPAPIInstance3D*>(PPAPI_GetInstanceObject());
}


void *PPAPIInstance3D::RenderLoop(void *this_ptr) {
  PPAPIInstance3D *pInst = static_cast<PPAPIInstance3D*>(this_ptr);
  pInst->render_loop_.AttachToCurrentThread();
  pInst->render_loop_.Run();
  return NULL;
}


void PPAPIInstance3D::Swapped(int result) {
  if (result != 0) {
    printf("Swapped result=%d.\n", result);
  }

  if (is_context_bound_) {
    PPAPIRender(size_.width(), size_.height());
    int result;
    result = device_context_.SwapBuffers(callback_factory_.NewCallback(
                                         &PPAPIInstance3D::Swapped));
    if (result == PP_OK_COMPLETIONPENDING) return;
    printf("Failed swap with %d.\n", result);
  }

  // Failed to draw, so add a callback for the future.
  pp::MessageLoop::GetCurrent().PostWork(callback_factory_.NewCallback(
                                         &PPAPIInstance3D::Swapped), 100);
}

void PPAPIInstance3D::BuildContext(int32_t result, const pp::Size& new_size) {
  printf("Building context.\n");

  // If already bound, try to resize to avoid the need to rebuild the context.
  if (is_context_bound_) {
    // If the size is correct, then just skip this request.
    if (new_size == size_) {
      printf("Skipped building context, same size as bound.\n");
      return;
    }
    int err = device_context_.ResizeBuffers(new_size.width(),
                                            new_size.height());

    // Resized the context, we are done
    if (err == PP_OK) {
      size_ = new_size;
      fprintf(stderr, "Resized context from %dx%d to %dx%d",
              size_.width(), size_.height(), new_size.width(),
              new_size.height());
      PPAPIBuildContext(size_.width(), size_.height());
      return;
    }

    // Failed to resize, fall through and start from scratch
    fprintf(stderr, "Failed to resize buffer from %dx%d to %dx%d",
            size_.width(), size_.height(), new_size.width(), new_size.height());

    is_context_bound_ = false;
  }

  printf("Calling create context....\n");
  size_ = new_size;
  device_context_ = pp::Graphics3D(this, PPAPIGet3DAttribs(size_.width(),
                                                           size_.height()));
  printf("Got Context!\n");
  is_context_bound_ = BindGraphics(device_context_);
  printf("Context is bound=%d\n", is_context_bound_);

  // Set the context regardless to make sure we have a valid one
  glSetCurrentContextPPAPI(device_context_.pp_resource());
  if (is_context_bound_) {
    PPAPIBuildContext(size_.width(), size_.height());
    device_context_.SwapBuffers(callback_factory_.NewCallback(
                                &PPAPIInstance3D::Swapped));
  } else {
    fprintf(stderr, "Failed to bind context for %dx%d.\n", size_.width(),
            size_.height());
  }
}

PPAPIInstance3D::PPAPIInstance3D(PP_Instance instance, const char *args[])
    : PPAPIInstance(instance, args),
      mouse_locked_(false),
      callback_factory_(this),
      fullscreen_(this),
      is_context_bound_(false),
      was_fullscreen_(false),
      render_loop_(this),
      main_thread_3d_(true) {
  glInitializePPAPI(pp::Module::Get()->get_browser_interface());
}


PPAPIInstance3D::~PPAPIInstance3D() {
  if (is_context_bound_) {
    is_context_bound_ = false;
    // Cleanup code?
  }
}

bool PPAPIInstance3D::Init(uint32_t arg,
                           const char* argn[],
                           const char* argv[]) {
  if (!PPAPIInstance::Init(arg, argn, argv)) {
    return false;
  }

  for (uint32_t a=0; a < arg; a++) {
    printf("%s=%s\n", argn[a], argv[a]);
  }

  const char *use_main = GetProperty("pm_main3d", "true");
  main_thread_3d_ = !strcasecmp(use_main, "true");
  printf("Using 3D on main thread = %s.\n", use_main);
  if (!main_thread_3d_) {
    pthread_t render_thread;
    int ret = pthread_create(&render_thread, NULL, RenderLoop,
                             static_cast<void*>(this));
    return ret == 0;
  }
  return true;
}

void PPAPIInstance3D::DidChangeView(const pp::View& view) {
  pp::Size new_size = view.GetRect().size();
  printf("View changed: %dx%d\n", new_size.width(), new_size.height());

  // Build or update the 3D context when the view changes.
  if (main_thread_3d_) {
    // If using the main thread, update the context immediately
    BuildContext(0, new_size);
  } else {
    // If using a seperate thread, then post the message so we can build the
    // context on the correct thread.
    render_loop_.PostWork(callback_factory_.NewCallback(
                          &PPAPIInstance3D::BuildContext, new_size));
  }
}

bool PPAPIInstance3D::ToggleFullscreen() {
  // Ignore switch if in transition
  if (!is_context_bound_)
    return false;

  if (fullscreen_.IsFullscreen()) {
    if (!fullscreen_.SetFullscreen(false)) {
      printf("Could not leave fullscreen mode\n");
      return false;
    }
  } else {
    if (!fullscreen_.SetFullscreen(true)) {
      printf("Could not enter fullscreen mode\n");
      return false;
    }
  }
  return true;
}
