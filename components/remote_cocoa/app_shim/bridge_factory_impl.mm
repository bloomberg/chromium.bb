// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/bridge_factory_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/remote_cocoa/app_shim/alert.h"
#include "components/remote_cocoa/app_shim/bridged_native_widget_host_helper.h"
#include "components/remote_cocoa/app_shim/bridged_native_widget_impl.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/remote_accessibility_api.h"

namespace remote_cocoa {

using views::BridgedNativeWidgetImpl;
using views::BridgedNativeWidgetHostHelper;

namespace {

class Bridge : public BridgedNativeWidgetHostHelper {
 public:
  Bridge(uint64_t bridge_id,
         mojom::BridgedNativeWidgetAssociatedRequest bridge_request,
         mojom::BridgedNativeWidgetHostAssociatedPtrInfo host_ptr,
         mojom::TextInputHostAssociatedPtrInfo text_input_host_ptr) {
    host_ptr_.Bind(std::move(host_ptr),
                   ui::WindowResizeHelperMac::Get()->task_runner());
    text_input_host_ptr_.Bind(std::move(text_input_host_ptr),
                              ui::WindowResizeHelperMac::Get()->task_runner());
    bridge_impl_ = std::make_unique<BridgedNativeWidgetImpl>(
        bridge_id, host_ptr_.get(), this, text_input_host_ptr_.get());
    bridge_impl_->BindRequest(
        std::move(bridge_request),
        base::BindOnce(&Bridge::OnConnectionError, base::Unretained(this)));
  }

 private:
  ~Bridge() override {}

  void OnConnectionError() { delete this; }

  // BridgedNativeWidgetHostHelper:
  id GetNativeViewAccessible() override {
    if (!remote_accessibility_element_) {
      int64_t browser_pid = 0;
      std::vector<uint8_t> element_token;
      host_ptr_->GetRootViewAccessibilityToken(&browser_pid, &element_token);
      [NSAccessibilityRemoteUIElement
          registerRemoteUIProcessIdentifier:browser_pid];
      remote_accessibility_element_ =
          ui::RemoteAccessibility::GetRemoteElementFromToken(element_token);
    }
    return remote_accessibility_element_.get();
  }
  void DispatchKeyEvent(ui::KeyEvent* event) override {
    bool event_handled = false;
    host_ptr_->DispatchKeyEventRemote(std::make_unique<ui::KeyEvent>(*event),
                                      &event_handled);
    if (event_handled)
      event->SetHandled();
  }
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override {
    bool event_swallowed = false;
    bool event_handled = false;
    host_ptr_->DispatchKeyEventToMenuControllerRemote(
        std::make_unique<ui::KeyEvent>(*event), &event_swallowed,
        &event_handled);
    if (event_handled)
      event->SetHandled();
    return event_swallowed;
  }
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override {
    *found_word = false;
  }
  remote_cocoa::DragDropClient* GetDragDropClient() override {
    // Drag-drop only doesn't work across mojo yet.
    return nullptr;
  }
  ui::TextInputClient* GetTextInputClient() override {
    // Text input doesn't work across mojo yet.
    return nullptr;
  }

  mojom::BridgedNativeWidgetHostAssociatedPtr host_ptr_;
  mojom::TextInputHostAssociatedPtr text_input_host_ptr_;

  std::unique_ptr<BridgedNativeWidgetImpl> bridge_impl_;
  base::scoped_nsobject<NSAccessibilityRemoteUIElement>
      remote_accessibility_element_;
};

}  // namespace

// static
BridgeFactoryImpl* BridgeFactoryImpl::Get() {
  static base::NoDestructor<BridgeFactoryImpl> factory;
  return factory.get();
}

void BridgeFactoryImpl::BindRequest(
    mojom::BridgeFactoryAssociatedRequest request) {
  binding_.Bind(std::move(request),
                ui::WindowResizeHelperMac::Get()->task_runner());
}

void BridgeFactoryImpl::SetContentNSViewCreateCallbacks(
    RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback,
    WebContentsNSViewCreateCallback web_conents_create_callback) {
  render_widget_host_create_callback_ = render_widget_host_create_callback;
  web_conents_create_callback_ = web_conents_create_callback;
}

void BridgeFactoryImpl::CreateAlert(mojom::AlertBridgeRequest bridge_request) {
  // The resulting object manages its own lifetime.
  ignore_result(new AlertBridge(std::move(bridge_request)));
}

void BridgeFactoryImpl::CreateBridgedNativeWidget(
    uint64_t bridge_id,
    mojom::BridgedNativeWidgetAssociatedRequest bridge_request,
    mojom::BridgedNativeWidgetHostAssociatedPtrInfo host,
    mojom::TextInputHostAssociatedPtrInfo text_input_host) {
  // The resulting object will be destroyed when its message pipe is closed.
  ignore_result(new Bridge(bridge_id, std::move(bridge_request),
                           std::move(host), std::move(text_input_host)));
}

void BridgeFactoryImpl::CreateRenderWidgetHostNSView(
    mojom::StubInterfaceAssociatedPtrInfo host,
    mojom::StubInterfaceAssociatedRequest view_request) {
  if (!render_widget_host_create_callback_)
    return;
  render_widget_host_create_callback_.Run(host.PassHandle(),
                                          view_request.PassHandle());
}

void BridgeFactoryImpl::CreateWebContentsNSView(
    uint64_t view_id,
    mojom::StubInterfaceAssociatedPtrInfo host,
    mojom::StubInterfaceAssociatedRequest view_request) {
  if (!web_conents_create_callback_)
    return;
  web_conents_create_callback_.Run(view_id, host.PassHandle(),
                                   view_request.PassHandle());
}

BridgeFactoryImpl::BridgeFactoryImpl() : binding_(this) {}

BridgeFactoryImpl::~BridgeFactoryImpl() {}

}  // namespace remote_cocoa
