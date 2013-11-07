// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_controller.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {

NativeViewportController::NativeViewportController(
    shell::Context* context, Handle pipe)
    : pipe_(pipe) {
  native_viewport_ = NativeViewport::Create(context, this);
}
NativeViewportController::~NativeViewportController() {
}

void NativeViewportController::Close() {
  DCHECK(native_viewport_);
  native_viewport_->Close();
}

bool NativeViewportController::OnEvent(ui::Event* event) {
  ui::LocatedEvent* located = static_cast<ui::LocatedEvent*>(event);
  SendString(base::StringPrintf("Event @ %d,%d",
                                located->location().x(),
                                located->location().y()));
  return false;
}

void NativeViewportController::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  gfx::Size size = native_viewport_->GetSize();
  gpu::GLInProcessContextAttribs attribs;
  gl_context_.reset(gpu::GLInProcessContext::CreateContext(
      false, widget, size, false, attribs, gfx::PreferDiscreteGpu));
  gl_context_->SetContextLostCallback(base::Bind(
      &NativeViewportController::OnGLContextLost, base::Unretained(this)));

  // TODO(abarth): Instead of drawing green, we want to send the context over
  // pipe_ somehow.
  gpu::gles2::GLES2Interface* gl = gl_context_->GetImplementation();
  gl->ClearColor(0, 1, 0, 0);
  gl->Clear(GL_COLOR_BUFFER_BIT);
  gl->SwapBuffers();
}

void NativeViewportController::OnGLContextLost() {
  SendString("GL context lost");
}

void NativeViewportController::OnResized(const gfx::Size& size) {
  SendString(base::StringPrintf("Sized to: %d x %d",
                                size.width(),
                                size.height()));
}

void NativeViewportController::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

void NativeViewportController::SendString(const std::string& string) {
  WriteMessage(pipe_, string.c_str(), string.size()+1, NULL, 0,
                MOJO_WRITE_MESSAGE_FLAG_NONE);
}

}  // namespace services
}  // namespace mojo
