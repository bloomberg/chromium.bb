// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_NATIVE_VIEWPORT_EVENT_DISPATCHER_IMPL_H_
#define SERVICES_WINDOW_MANAGER_NATIVE_VIEWPORT_EVENT_DISPATCHER_IMPL_H_

#include "base/basictypes.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/mojo_services/src/native_viewport/public/interfaces/native_viewport.mojom.h"
#include "ui/events/event_source.h"

namespace window_manager {

class WindowManagerApp;

class NativeViewportEventDispatcherImpl
    : public ui::EventSource,
      public mojo::NativeViewportEventDispatcher {
 public:
  NativeViewportEventDispatcherImpl(
      WindowManagerApp* app,
      mojo::InterfaceRequest<mojo::NativeViewportEventDispatcher> request);
  ~NativeViewportEventDispatcherImpl() override;

 private:
  // ui::EventSource:
  ui::EventProcessor* GetEventProcessor() override;

  // NativeViewportEventDispatcher:
  void OnEvent(mojo::EventPtr event,
               const mojo::Callback<void()>& callback) override;

  WindowManagerApp* app_;

  mojo::StrongBinding<mojo::NativeViewportEventDispatcher> binding_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportEventDispatcherImpl);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_NATIVE_VIEWPORT_EVENT_DISPATCHER_IMPL_H_
