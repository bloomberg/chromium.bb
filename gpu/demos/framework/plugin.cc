// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/plugin.h"

#include <cassert>
#include "gpu/demos/framework/demo_factory.h"

using gpu::demos::Plugin;

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
NPExtensions* g_extensions = NULL;

// Plugin class functions.
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

void TickCallback(void* data) {
  reinterpret_cast<gpu::demos::Plugin*>(data)->Tick();
}

void RepaintCallback(NPP npp, NPDeviceContext3D* /* context */) {
  Plugin* plugin = static_cast<Plugin*>(npp->pdata);
  plugin->Paint();
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
  pglMakeCurrent(PGL_NO_CONTEXT);

  DestroyContext();
}

NPClass* Plugin::GetPluginClass() {
  return &plugin_class;
}

void Plugin::New(NPMIMEType pluginType,
                 int16 argc, char* argn[], char* argv[]) {
  if (!g_extensions) {
    g_browser->getvalue(npp_, NPNVPepperExtensions, &g_extensions);
    assert(g_extensions);
  }

  device3d_ = g_extensions->acquireDevice(npp_, NPPepper3DDevice);
  assert(device3d_);
}

void Plugin::SetWindow(const NPWindow& window) {
  demo_->InitWindowSize(window.width, window.height);

  if (!pgl_context_) {
    if (!CreateContext())
      return;

    // Schedule first call to Tick.
    if (demo_->IsAnimated())
      g_browser->pluginthreadasynccall(npp_, TickCallback, this);
  }
}

int32 Plugin::HandleEvent(const NPPepperEvent& event) {
  return 0;
}

void Plugin::Tick() {
  Paint();

  // Schedule another call to Tick.
  g_browser->pluginthreadasynccall(npp_, TickCallback, this);
}

void Plugin::Paint() {
  if (!pglMakeCurrent(pgl_context_) && pglGetError() == PGL_CONTEXT_LOST) {
    DestroyContext();
    if (!CreateContext())
      return;

    pglMakeCurrent(pgl_context_);
  }

  demo_->Draw();
  pglSwapBuffers();
  pglMakeCurrent(PGL_NO_CONTEXT);
}

bool Plugin::CreateContext() {
  assert(!pgl_context_);

  // Initialize a 3D context.
  NPDeviceContext3DConfig config;
  config.commandBufferSize = kCommandBufferSize;
  if (NPERR_NO_ERROR != device3d_->initializeContext(npp_,
                                                     &config,
                                                     &context3d_)) {
    DestroyContext();
    return false;
  }

  context3d_.repaintCallback = RepaintCallback;

  // Create a PGL context.
  pgl_context_ = pglCreateContext(npp_, device3d_, &context3d_);
  if (!pgl_context_) {
    DestroyContext();
    return false;
  }

  // Initialize demo.
  pglMakeCurrent(pgl_context_);
  if (!demo_->InitGL()) {
    DestroyContext();
    return false;
  }

  pglMakeCurrent(PGL_NO_CONTEXT);

  return true;
}

void Plugin::DestroyContext() {
  if (pgl_context_) {
    pglDestroyContext(pgl_context_);
    pgl_context_ = NULL;
  }

  if (context3d_.commandBuffer) {
    device3d_->destroyContext(npp_, &context3d_);
    memset(&context3d_, 0, sizeof(context3d_));
  }
}

}  // namespace demos
}  // namespace gpu
