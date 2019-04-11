// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/legacy_frame_bridge.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/engine/legacy_message_port_bridge.h"
#include "fuchsia/engine/legacy_navigation_controller_bridge.h"
#include "fuchsia/engine/legacy_navigation_event_listener_bridge.h"

namespace {

// All the active LegacyFrameBridge instances. Used for testing.
std::set<LegacyFrameBridge*> g_legacy_bridges;

}  // namespace

LegacyFrameBridge::LegacyFrameBridge(
    fidl::InterfaceRequest<chromium::web::Frame> request,
    fuchsia::web::FramePtr handle)
    : binding_(this, std::move(request)), frame_(std::move(handle)) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |binding_| disconnected.";
    delete this;
  });
  frame_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |frame_| disconnected.";
    delete this;
  });
  g_legacy_bridges.insert(this);
}

// static
fuchsia::web::FramePtr* LegacyFrameBridge::GetFramePtrForTest(
    chromium::web::FramePtr* frame_ptr) {
  // Find the LegacyFrameBridge whose channel is connected to |frame_ptr| by
  // inspecting the related_koids of active LegacyFrameBridges.
  zx_info_handle_basic_t handle_info;
  zx_status_t status = frame_ptr->channel().get_info(
      ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
      nullptr, nullptr);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";
  zx_handle_t client_handle_koid = handle_info.koid;

  for (LegacyFrameBridge* legacy_frame : g_legacy_bridges) {
    status = legacy_frame->binding_.channel().get_info(
        ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
        nullptr, nullptr);
    ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";

    if (client_handle_koid == handle_info.related_koid)
      return &legacy_frame->frame_;
  }

  return nullptr;
}

LegacyFrameBridge::~LegacyFrameBridge() {
  g_legacy_bridges.erase(this);
}

void LegacyFrameBridge::CreateView(fuchsia::ui::views::ViewToken view_token) {
  frame_->CreateView(std::move(view_token));
}

void LegacyFrameBridge::CreateView2(
    zx::eventpair view_token_value,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  fuchsia::ui::views::ViewToken view_token;
  view_token.value = std::move(view_token_value);
  CreateView(std::move(view_token));
}

void LegacyFrameBridge::GetNavigationController(
    fidl::InterfaceRequest<chromium::web::NavigationController> controller) {
  fuchsia::web::NavigationControllerPtr fuchsia_controller;
  frame_->GetNavigationController(fuchsia_controller.NewRequest());
  new LegacyNavigationControllerBridge(std::move(controller),
                                       std::move(fuchsia_controller));
}

void LegacyFrameBridge::ExecuteJavaScript(std::vector<std::string> origins,
                                          fuchsia::mem::Buffer script,
                                          chromium::web::ExecuteMode mode,
                                          ExecuteJavaScriptCallback callback) {
  if (mode == chromium::web::ExecuteMode::ON_PAGE_LOAD) {
    AddJavaScriptBindings(next_transitional_bindings_id_++, std::move(origins),
                          std::move(script), std::move(callback));
    return;
  } else if (mode != chromium::web::ExecuteMode::IMMEDIATE_ONCE) {
    // Unknown mode, ignored.
    callback(false);
    return;
  }

  frame_->ExecuteJavaScriptNoResult(
      std::move(origins), std::move(script),
      [callback = std::move(callback)](
          fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        callback(result.is_response());
      });
}

void LegacyFrameBridge::AddJavaScriptBindings(
    uint64_t id,
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    AddJavaScriptBindingsCallback callback) {
  frame_->AddBeforeLoadJavaScript(
      id, std::move(origins), std::move(script),
      [callback = std::move(callback)](
          fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        callback(result.is_response());
      });
}

void LegacyFrameBridge::RemoveJavaScriptBindings(
    uint64_t id,
    RemoveJavaScriptBindingsCallback callback) {
  frame_->RemoveBeforeLoadJavaScript(id);
  callback(true);
}

void LegacyFrameBridge::PostMessage(
    chromium::web::WebMessage message,
    std::string target_origin,
    chromium::web::Frame::PostMessageCallback callback) {
  base::Optional<fuchsia::web::WebMessage> converted =
      LegacyMessagePortBridge::ConvertFromLegacyWebMessage(message);
  if (!converted) {
    callback(false);
    return;
  }
  frame_->PostMessage(std::move(target_origin), std::move(converted.value()),
                      [callback = std::move(callback)](
                          fuchsia::web::Frame_PostMessage_Result result) {
                        callback(result.is_response());
                      });
}

void LegacyFrameBridge::SetNavigationEventObserver(
    fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer) {
  if (observer) {
    fuchsia::web::NavigationEventListenerPtr listener;
    new LegacyNavigationEventListenerBridge(listener.NewRequest(),
                                            observer.Bind());
    frame_->SetNavigationEventListener(std::move(listener));
  } else {
    frame_->SetNavigationEventListener(nullptr);
  }
}

void LegacyFrameBridge::SetJavaScriptLogLevel(chromium::web::LogLevel level) {
  frame_->SetJavaScriptLogLevel(
      static_cast<fuchsia::web::ConsoleLogLevel>(level));
}

void LegacyFrameBridge::SetEnableInput(bool enable_input) {
  frame_->SetEnableInput(enable_input);
}
