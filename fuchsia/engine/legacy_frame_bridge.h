// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_LEGACY_FRAME_BRIDGE_H_
#define FUCHSIA_ENGINE_LEGACY_FRAME_BRIDGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

// Allows chromium::web::Frame clients to connect to fuchsia::web::Frame
// instances.
//
// LegacyFrameBridge instances are self-managed; they destroy themselves
// when the connection with either end is terminated.
class WEB_ENGINE_EXPORT LegacyFrameBridge : public chromium::web::Frame {
 public:
  LegacyFrameBridge(fidl::InterfaceRequest<chromium::web::Frame> request,
                    fuchsia::web::FramePtr handle);

  static fuchsia::web::FramePtr* GetFramePtrForTest(
      chromium::web::FramePtr* frame_ptr);

 private:
  // Non-public to ensure that only this object may destroy itself.
  ~LegacyFrameBridge() override;

  // chromium::web::Frame implementation.
  void CreateView(fuchsia::ui::views::ViewToken view_token) override;
  void CreateView2(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override;
  void GetNavigationController(
      fidl::InterfaceRequest<chromium::web::NavigationController> controller)
      override;
  void ExecuteJavaScript(std::vector<std::string> origins,
                         fuchsia::mem::Buffer script,
                         chromium::web::ExecuteMode mode,
                         ExecuteJavaScriptCallback callback) override;
  void AddJavaScriptBindings(uint64_t id,
                             std::vector<std::string> origins,
                             fuchsia::mem::Buffer script,
                             AddJavaScriptBindingsCallback callback) override;
  void RemoveJavaScriptBindings(
      uint64_t id,
      RemoveJavaScriptBindingsCallback callback) override;
  void PostMessage(chromium::web::WebMessage message,
                   std::string target_origin,
                   PostMessageCallback callback) override;
  void SetNavigationEventObserver(
      fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer)
      override;
  void SetJavaScriptLogLevel(chromium::web::LogLevel level) override;
  void SetEnableInput(bool enable_input) override;

  fidl::Binding<chromium::web::Frame> binding_;
  fuchsia::web::FramePtr frame_;
  uint64_t next_transitional_bindings_id_ = 0x80000000;

  DISALLOW_COPY_AND_ASSIGN(LegacyFrameBridge);
};

#endif  // FUCHSIA_ENGINE_LEGACY_FRAME_BRIDGE_H_
