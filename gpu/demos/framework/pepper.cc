// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/scoped_ptr.h"
#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/dev/graphics_3d_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

namespace gpu {
namespace demos {

class PluginInstance : public pp::Instance {
 public:
  PluginInstance(PP_Instance instance, pp::Module* module)
      : pp::Instance(instance),
        module_(module),
        demo_(CreateDemo()) {
    // Set the callback object outside of the initializer list to avoid a
    // compiler warning about using "this" in an initializer list.
    callback_factory_.Initialize(this);
  }

  ~PluginInstance() {
    if (!graphics_.is_null()) {
      graphics_.MakeCurrent();
      demo_.reset();
      pp::Graphics3D_Dev::ResetCurrent();
    }
  }

  virtual void ViewChanged(const pp::Rect& position, const pp::Rect& /*clip*/) {
    if (size_.IsEmpty() && !position.IsEmpty()) {
      size_ = position.size();
      demo_->InitWindowSize(size_.width(), size_.height());
      graphics_ = pp::Graphics3D_Dev(*this, 0, NULL, NULL);
      if (!graphics_.is_null()) {
        graphics_.MakeCurrent();
        demo_->InitGL();
        pp::Graphics3D_Dev::ResetCurrent();

        // TODO(neb): Remove this once the startup order bug (51842) is fixed.
        if (true)
  //      if (demo_->IsAnimated())
          Animate(0);
        else
          Paint();
      }
    }
  }

  virtual void Graphics3DContextLost() {
    // TODO(neb): Replace this with the correct code once 53889 is fixed.
    Paint();
//    pp::Rect fake_position(size_);
//    size_ = pp::Size();
//    ViewChanged(fake_position, fake_position);
  }

  void Paint() {
    graphics_.MakeCurrent();
    demo_->Draw();
    graphics_.SwapBuffers();
    pp::Graphics3D_Dev::ResetCurrent();
  }

 private:
  void Animate(int delay) {
    Paint();
    module_->core()->CallOnMainThread(delay,
        callback_factory_.NewCallback(&PluginInstance::Animate), delay);
  }

  pp::Module* module_;
  scoped_ptr<Demo> demo_;
  pp::Graphics3D_Dev graphics_;
  pp::Size size_;
  pp::CompletionCallbackFactory<PluginInstance> callback_factory_;
};

class PluginModule : public pp::Module {
 public:
  PluginModule() : pp::Module(), at_exit_manager_(new base::AtExitManager) {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new PluginInstance(instance, this);
  }

 private:
  scoped_ptr<base::AtExitManager> at_exit_manager_;
};

}  // namespace demos
}  // namespace gpu

namespace pp {

Module* CreateModule() {
  return new gpu::demos::PluginModule();
}

}  // namespace pp

