// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/plugin.h"

#include "base/logging.h"
#include "gpu/demos/framework/demo_factory.h"

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
NPExtensions* g_extensions = NULL;

// Plugin class functions.
using gpu::demos::Plugin;
NPObject* PluginAllocate(NPP npp, NPClass* the_class) {
  Plugin* plugin = new Plugin(npp);
  return plugin;
}

void PluginDeallocate(NPObject* header) {
  Plugin* plugin = static_cast<Plugin*>(header);
  delete plugin;
}

void PluginInvalidate(NPObject* obj) {
}

bool PluginHasMethod(NPObject* obj, NPIdentifier name) {
  return false;
}

bool PluginInvoke(NPObject* header,
                  NPIdentifier name,
                  const NPVariant* args, uint32 arg_count,
                  NPVariant* result) {
  return false;
}

bool PluginInvokeDefault(NPObject* obj,
                         const NPVariant* args, uint32 arg_count,
                         NPVariant* result) {
  VOID_TO_NPVARIANT(*result);
  return true;
}

bool PluginHasProperty(NPObject* obj, NPIdentifier name) {
  return false;
}

bool PluginGetProperty(NPObject* obj,
                       NPIdentifier name,
                       NPVariant* result) {
  return false;
}

bool PluginSetProperty(NPObject* obj,
                       NPIdentifier name,
                       const NPVariant* variant) {
  return false;
}

NPClass plugin_class = {
  NP_CLASS_STRUCT_VERSION,
  PluginAllocate,
  PluginDeallocate,
  PluginInvalidate,
  PluginHasMethod,
  PluginInvoke,
  PluginInvokeDefault,
  PluginHasProperty,
  PluginGetProperty,
  PluginSetProperty,
};

void PaintCallback(void* data) {
  reinterpret_cast<gpu::demos::Plugin*>(data)->Paint();
}
}

namespace gpu {
namespace demos {

NPNetscapeFuncs* g_browser;

Plugin::Plugin(NPP npp)
  : npp_(npp),
    device3d_(NULL),
    pgl_context_(NULL),
    demo_(CreateDemo()) {
  memset(&context3d_, 0, sizeof(context3d_));
}

Plugin::~Plugin() {
  // Destroy demo while GL context is current and before it is destroyed.
  pglMakeCurrent(pgl_context_);
  demo_.reset();
  pglMakeCurrent(NULL);

  pglDestroyContext(pgl_context_);
}

NPClass* Plugin::GetPluginClass() {
  return &plugin_class;
}

void Plugin::New(NPMIMEType pluginType,
                 int16 argc, char* argn[], char* argv[]) {
  if (!g_extensions) {
    g_browser->getvalue(npp_, NPNVPepperExtensions, &g_extensions);
    CHECK(g_extensions);
  }

  device3d_ = g_extensions->acquireDevice(npp_, NPPepper3DDevice);
  CHECK(device3d_);
}

void Plugin::SetWindow(const NPWindow& window) {
  if (!pgl_context_) {
    // Initialize a 3D context.
    NPDeviceContext3DConfig config;
    config.commandBufferSize = kCommandBufferSize;
    device3d_->initializeContext(npp_, &config, &context3d_);

    // Create a PGL context.
    pgl_context_ = pglCreateContext(npp_, device3d_, &context3d_);

    // Initialize demo.
    pglMakeCurrent(pgl_context_);
    demo_->InitWindowSize(window.width, window.height);
    CHECK(demo_->InitGL());
    pglMakeCurrent(NULL);
  }

  // Schedule the first call to Draw.
  g_browser->pluginthreadasynccall(npp_, PaintCallback, this);
}

void Plugin::Paint() {
  // Render some stuff.
  pglMakeCurrent(pgl_context_);
  demo_->Draw();
  pglSwapBuffers();
  pglMakeCurrent(NULL);

  // Schedule another call to Paint.
  g_browser->pluginthreadasynccall(npp_, PaintCallback, this);
}

}  // namespace demos
}  // namespace gpu
