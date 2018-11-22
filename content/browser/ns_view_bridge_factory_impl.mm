// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ns_view_bridge_factory_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/browser/renderer_host/render_widget_host_ns_view_bridge_local.h"
#include "content/browser/renderer_host/render_widget_host_ns_view_client_helper.h"
#include "content/browser/web_contents/web_contents_ns_view_bridge.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

namespace content {

namespace {

class RenderWidgetHostNSViewBridgeOwner
    : public RenderWidgetHostNSViewClientHelper {
 public:
  explicit RenderWidgetHostNSViewBridgeOwner(
      mojom::RenderWidgetHostNSViewClientAssociatedPtr client,
      mojom::RenderWidgetHostNSViewBridgeAssociatedRequest bridge_request)
      : client_(std::move(client)) {
    bridge_ = std::make_unique<RenderWidgetHostNSViewBridgeLocal>(client_.get(),
                                                                  this);
    bridge_->BindRequest(std::move(bridge_request));
    client_.set_connection_error_handler(
        base::BindOnce(&RenderWidgetHostNSViewBridgeOwner::OnConnectionError,
                       base::Unretained(this)));
  }

 private:
  void OnConnectionError() { delete this; }

  std::unique_ptr<InputEvent> TranslateEvent(
      const blink::WebInputEvent& web_event) {
    return std::make_unique<InputEvent>(web_event, ui::LatencyInfo());
  }

  // RenderWidgetHostNSViewClientHelper implementation.
  id GetRootBrowserAccessibilityElement() override { return nil; }
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                            const ui::LatencyInfo& latency_info) override {
    const blink::WebKeyboardEvent* web_event =
        static_cast<const blink::WebKeyboardEvent*>(&key_event);
    std::unique_ptr<InputEvent> input_event =
        std::make_unique<InputEvent>(*web_event, latency_info);
    client_->ForwardKeyboardEvent(std::move(input_event),
                                  key_event.skip_in_browser);
  }
  void ForwardKeyboardEventWithCommands(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      const std::vector<EditCommand>& commands) override {
    const blink::WebKeyboardEvent* web_event =
        static_cast<const blink::WebKeyboardEvent*>(&key_event);
    std::unique_ptr<InputEvent> input_event =
        std::make_unique<InputEvent>(*web_event, latency_info);
    client_->ForwardKeyboardEventWithCommands(
        std::move(input_event), key_event.skip_in_browser, commands);
  }
  void RouteOrProcessMouseEvent(
      const blink::WebMouseEvent& web_event) override {
    client_->RouteOrProcessMouseEvent(TranslateEvent(web_event));
  }
  void RouteOrProcessTouchEvent(
      const blink::WebTouchEvent& web_event) override {
    client_->RouteOrProcessTouchEvent(TranslateEvent(web_event));
  }
  void RouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) override {
    client_->RouteOrProcessWheelEvent(TranslateEvent(web_event));
  }
  void ForwardMouseEvent(const blink::WebMouseEvent& web_event) override {
    client_->ForwardMouseEvent(TranslateEvent(web_event));
  }
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& web_event) override {
    client_->ForwardWheelEvent(TranslateEvent(web_event));
  }
  void GestureBegin(blink::WebGestureEvent begin_event,
                    bool is_synthetically_injected) override {
    // The gesture type is not yet known, but assign a type to avoid
    // serialization asserts (the type will be stripped on the other side).
    begin_event.SetType(blink::WebInputEvent::kGestureScrollBegin);
    client_->GestureBegin(TranslateEvent(begin_event),
                          is_synthetically_injected);
  }
  void GestureUpdate(blink::WebGestureEvent update_event) override {
    client_->GestureUpdate(TranslateEvent(update_event));
  }
  void GestureEnd(blink::WebGestureEvent end_event) override {
    client_->GestureEnd(TranslateEvent(end_event));
  }
  void SmartMagnify(const blink::WebGestureEvent& web_event) override {
    client_->SmartMagnify(TranslateEvent(web_event));
  }

  mojom::RenderWidgetHostNSViewClientAssociatedPtr client_;
  std::unique_ptr<RenderWidgetHostNSViewBridgeLocal> bridge_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewBridgeOwner);
};

}

// static
NSViewBridgeFactoryImpl* NSViewBridgeFactoryImpl::Get() {
  static base::NoDestructor<NSViewBridgeFactoryImpl> instance;
  return instance.get();
}

void NSViewBridgeFactoryImpl::BindRequest(
    mojom::NSViewBridgeFactoryAssociatedRequest request) {
  binding_.Bind(std::move(request),
                ui::WindowResizeHelperMac::Get()->task_runner());
}

void NSViewBridgeFactoryImpl::CreateRenderWidgetHostNSViewBridge(
    mojom::StubInterfaceAssociatedPtrInfo stub_client,
    mojom::StubInterfaceAssociatedRequest stub_bridge_request) {
  // Cast from the stub interface to the mojom::RenderWidgetHostNSViewClient
  // and mojom::RenderWidgetHostNSViewBridge private interfaces.
  // TODO(ccameron): Remove the need for this cast.
  // https://crbug.com/888290
  mojom::RenderWidgetHostNSViewClientAssociatedPtr client(
      mojo::AssociatedInterfacePtrInfo<mojom::RenderWidgetHostNSViewClient>(
          stub_client.PassHandle(), 0));
  mojom::RenderWidgetHostNSViewBridgeAssociatedRequest bridge_request(
      stub_bridge_request.PassHandle());

  // Create a RenderWidgetHostNSViewBridgeOwner. The resulting object will be
  // destroyed when its underlying pipe is closed.
  ignore_result(new RenderWidgetHostNSViewBridgeOwner(
      std::move(client), std::move(bridge_request)));
}

void NSViewBridgeFactoryImpl::CreateWebContentsNSViewBridge(
    uint64_t view_id,
    mojom::WebContentsNSViewClientAssociatedPtrInfo client,
    mojom::WebContentsNSViewBridgeAssociatedRequest bridge_request) {
  // Note that the resulting object will be destroyed when its underlying pipe
  // is closed.
  ignore_result(new WebContentsNSViewBridge(
      view_id, mojom::WebContentsNSViewClientAssociatedPtr(std::move(client)),
      std::move(bridge_request)));
}

NSViewBridgeFactoryImpl::NSViewBridgeFactoryImpl() : binding_(this) {}

NSViewBridgeFactoryImpl::~NSViewBridgeFactoryImpl() {}

}  // namespace content
