// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ns_view_bridge_factory_impl.h"

#include "base/macros.h"
#include "base/no_destructor.h"
#include "content/browser/renderer_host/render_widget_host_ns_view_bridge_local.h"

namespace content {

// static
NSViewBridgeFactoryImpl* NSViewBridgeFactoryImpl::Get() {
  static base::NoDestructor<NSViewBridgeFactoryImpl> instance;
  return instance.get();
}

void NSViewBridgeFactoryImpl::BindRequest(
    mojom::NSViewBridgeFactoryAssociatedRequest request) {
  binding_.Bind(std::move(request));
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

  // Create a RenderWidgetHostNSViewBridgeLocal. The resulting object will be
  // destroyed when its underlying pipe is closed.
  ignore_result(new RenderWidgetHostNSViewBridgeLocal(
      std::move(client), std::move(bridge_request)));
}

NSViewBridgeFactoryImpl::NSViewBridgeFactoryImpl() : binding_(this) {}

NSViewBridgeFactoryImpl::~NSViewBridgeFactoryImpl() {}

}  // namespace content
