// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>

#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

namespace gpu {
namespace demos {

class PluginInstance : public pp::Instance {
 public:
  PluginInstance(PP_Instance instance, pp::Module* module)
      : pp::Instance(instance),
        module_(module),
        demo_(CreateDemo()),
        swap_pending_(false),
        paint_needed_(false),
        resize_needed_(false) {
    // Set the callback object outside of the initializer list to avoid a
    // compiler warning about using "this" in an initializer list.
    callback_factory_.Initialize(this);
  }

  ~PluginInstance() {
    if (!context_.is_null()) {
      glSetCurrentContextPPAPI(context_.pp_resource());
      delete demo_;
      glSetCurrentContextPPAPI(0);
    }
  }

  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& /*clip*/) {
    if (size_ == position.size())
      return;

    size_ = position.size();

    if (context_.is_null())
      CreateContext();
    else
      resize_needed_ = true;

    Paint();
  }

  void Paint() {
    if (swap_pending_) {
      // A swap is pending. Delay paint until swap finishes.
      paint_needed_ = true;
      return;
    }

    if (resize_needed_) {
      context_.ResizeBuffers(size_.width(), size_.height());
      demo_->Resize(size_.width(), size_.height());
      resize_needed_ = false;
    }

    glSetCurrentContextPPAPI(context_.pp_resource());
    demo_->Draw();
    glSetCurrentContextPPAPI(0);

    swap_pending_ = true;
    context_.SwapBuffers(
        callback_factory_.NewCallback(&PluginInstance::OnSwap));
  }

 private:
  void CreateContext() {
    int32_t attribs[] = {
        PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
        PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
        PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
        PP_GRAPHICS3DATTRIB_SAMPLES, 0,
        PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
        PP_GRAPHICS3DATTRIB_WIDTH, size_.width(),
        PP_GRAPHICS3DATTRIB_HEIGHT, size_.height(),
        PP_GRAPHICS3DATTRIB_NONE
    };
    context_ = pp::Graphics3D(*this, pp::Graphics3D(), attribs);
    if (context_.is_null())
      return;

    pp::Instance::BindGraphics(context_);

    glSetCurrentContextPPAPI(context_.pp_resource());
    demo_->InitGL();
    demo_->Resize(size_.width(), size_.height());
    glSetCurrentContextPPAPI(0);
  }

  void OnSwap(int32_t) {
    swap_pending_ = false;
    if (paint_needed_ || demo_->IsAnimated()) {
      paint_needed_ = false;
      Paint();
    }
  }

  pp::Module* module_;
  Demo* demo_;
  pp::Graphics3D context_;
  pp::Size size_;
  bool swap_pending_;
  bool paint_needed_;
  bool resize_needed_;
  pp::CompletionCallbackFactory<PluginInstance> callback_factory_;
};

class PluginModule : public pp::Module {
 public:
  PluginModule() {}
  ~PluginModule() {
    glTerminatePPAPI();
  }

  virtual bool Init() {
    return glInitializePPAPI(get_browser_interface()) == GL_TRUE ? true : false;
  }

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new PluginInstance(instance, this);
  }
};

}  // namespace demos
}  // namespace gpu

namespace pp {

Module* CreateModule() {
  return new gpu::demos::PluginModule();
}

}  // namespace pp

