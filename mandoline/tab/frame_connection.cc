// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame_connection.h"

#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "components/view_manager/public/interfaces/display.mojom.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/network/public/interfaces/cookie_store.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/interfaces/font_service.mojom.h"
#endif

namespace mandoline {

FrameConnection::FrameConnection() : application_connection_(nullptr) {
}

FrameConnection::~FrameConnection() {
}

void FrameConnection::Init(mojo::ApplicationImpl* app,
                           mojo::URLRequestPtr request,
                           mojo::ViewManagerClientPtr* view_manage_client) {
  DCHECK(!application_connection_);

  mojo::CapabilityFilterPtr filter(mojo::CapabilityFilter::New());
  mojo::Array<mojo::String> resource_provider_interfaces;
  resource_provider_interfaces.push_back(
      resource_provider::ResourceProvider::Name_);
  filter->filter.insert("mojo:resource_provider",
                        resource_provider_interfaces.Pass());

  mojo::Array<mojo::String> network_service_interfaces;
  network_service_interfaces.push_back(mojo::CookieStore::Name_);
  network_service_interfaces.push_back(mojo::URLLoaderFactory::Name_);
  network_service_interfaces.push_back(mojo::WebSocketFactory::Name_);
  filter->filter.insert("mojo:network_service",
                        network_service_interfaces.Pass());

  mojo::Array<mojo::String> clipboard_interfaces;
  clipboard_interfaces.push_back(mojo::Clipboard::Name_);
  filter->filter.insert("mojo:clipboard", clipboard_interfaces.Pass());

  mojo::Array<mojo::String> surfaces_interfaces;
  surfaces_interfaces.push_back(mojo::DisplayFactory::Name_);
  surfaces_interfaces.push_back(mojo::Surface::Name_);
  filter->filter.insert("mojo:surfaces_service", surfaces_interfaces.Pass());

  mojo::Array<mojo::String> view_manager_interfaces;
  view_manager_interfaces.push_back(mojo::Gpu::Name_);
  view_manager_interfaces.push_back(mojo::ViewManagerRoot::Name_);
  filter->filter.insert("mojo:view_manager", view_manager_interfaces.Pass());

  mojo::Array<mojo::String> devtools_interfaces;
  devtools_interfaces.push_back(devtools_service::DevToolsRegistry::Name_);
  filter->filter.insert("mojo:devtools_service", devtools_interfaces.Pass());

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  mojo::Array<mojo::String> font_service_interfaces;
  font_service_interfaces.push_back(font_service::FontService::Name_);
  filter->filter.insert("mojo:font_service", font_service_interfaces.Pass());
#endif

  application_connection_ = app->ConnectToApplicationWithCapabilityFilter(
      request.Pass(), filter.Pass());
  application_connection_->ConnectToService(view_manage_client);
  application_connection_->ConnectToService(&frame_tree_client_);
  frame_tree_client_.set_connection_error_handler([]() {
    // TODO(sky): implement this.
    NOTIMPLEMENTED();
  });
}

}  // namespace mandoline
