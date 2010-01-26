// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_DEMOS_FRAMEWORK_PLUGIN_H_
#define GPU_DEMOS_FRAMEWORK_PLUGIN_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "gpu/demos/framework/demo.h"
#include "gpu/pgl/pgl.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace gpu {
namespace demos {

// Acts as a framework for pepper3d demos. It is in fact a pepper plugin with
// a pepper3d device. It delegates all rendering tasks to demo object.
class Plugin : public NPObject {
 public:
  explicit Plugin(NPP npp);
  ~Plugin();

  static NPClass* GetPluginClass();

  NPP npp() const { return npp_; }
  void New(NPMIMEType pluginType, int16 argc, char* argn[], char* argv[]);
  void SetWindow(const NPWindow& window);

  // Called by the browser to paint the window.
  void Paint();

 private:
  // This class object needs to be safely casted to NPObject* and cross
  // c-c++ module boundaries. To accomplish that this class should not have
  // any virtual member function.
  NPP npp_;

  NPDevice* device3d_;
  NPDeviceContext3D context3d_;
  PGLContext pgl_context_;
  scoped_ptr<Demo> demo_;

  DISALLOW_COPY_AND_ASSIGN(Plugin);
};

extern NPNetscapeFuncs* g_browser;

}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_FRAMEWORK_PLUGIN_H_
