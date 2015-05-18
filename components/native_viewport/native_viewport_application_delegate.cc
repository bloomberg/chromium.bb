// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/native_viewport/native_viewport_application_delegate.h"

#include "base/command_line.h"
#include "components/native_viewport/native_viewport_impl.h"
#include "components/native_viewport/public/cpp/args.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"

namespace native_viewport {

NativeViewportApplicationDelegate::NativeViewportApplicationDelegate()
    : is_headless_(false) {
}

NativeViewportApplicationDelegate::~NativeViewportApplicationDelegate() {
}

void NativeViewportApplicationDelegate::Initialize(
    mojo::ApplicationImpl* application) {
  tracing_.Initialize(application);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  is_headless_ = command_line->HasSwitch(mojo::kUseHeadlessConfig);
  if (!is_headless_) {
    event_source_ = ui::PlatformEventSource::CreateDefault();
    if (command_line->HasSwitch(mojo::kUseTestConfig))
      gfx::GLSurface::InitializeOneOffForTests();
    else
      gfx::GLSurface::InitializeOneOff();
  }
}

bool NativeViewportApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mojo::NativeViewport>(this);
  connection->AddService<mojo::Gpu>(this);
  return true;
}

void NativeViewportApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::NativeViewport> request) {
  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  new NativeViewportImpl(is_headless_, gpu_state_, request.Pass());
}

void NativeViewportApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::Gpu> request) {
  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  new gles2::GpuImpl(request.Pass(), gpu_state_);
}

}  // namespace native_viewport
