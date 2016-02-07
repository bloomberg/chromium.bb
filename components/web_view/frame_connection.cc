// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame_connection.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_utils.h"
#include "components/web_view/test_runner/public/interfaces/layout_test_runner.mojom.h"
#include "mojo/services/network/public/interfaces/cookie_store.mojom.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/interfaces/font_service.mojom.h"
#endif

namespace web_view {
namespace {

// Callback from when the content handler id is obtained.
void OnGotContentHandlerForFrame(
    const uint32_t existing_content_handler_id,
    const FrameTreeDelegate::CanNavigateFrameCallback& callback,
    scoped_ptr<FrameConnection> connection) {
  mus::mojom::WindowTreeClientPtr window_tree_client;
  if (!AreAppIdsEqual(existing_content_handler_id,
                      connection->GetContentHandlerID())) {
    window_tree_client = connection->GetWindowTreeClient();
  }
  FrameConnection* connection_ptr = connection.get();
  callback.Run(connection_ptr->GetContentHandlerID(),
               connection_ptr->frame_client(), std::move(connection),
               std::move(window_tree_client));
}

}  // namespace

FrameConnection::FrameConnection() : connection_(nullptr) {
}

FrameConnection::~FrameConnection() {
}

// static
void FrameConnection::CreateConnectionForCanNavigateFrame(
    mojo::Shell* shell,
    Frame* frame,
    mojo::URLRequestPtr request,
    const FrameTreeDelegate::CanNavigateFrameCallback& callback) {
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  FrameConnection* connection = frame_connection.get();
  connection->Init(shell, std::move(request),
                   base::Bind(&OnGotContentHandlerForFrame, frame->app_id(),
                              callback, base::Passed(&frame_connection)));
}

void FrameConnection::Init(mojo::Shell* shell,
                           mojo::URLRequestPtr request,
                           const base::Closure& on_got_id_callback) {
  DCHECK(!connection_);

  mojo::shell::mojom::CapabilityFilterPtr filter(
      mojo::shell::mojom::CapabilityFilter::New());
  mojo::Array<mojo::String> resource_provider_interfaces;
  resource_provider_interfaces.push_back(
      resource_provider::ResourceProvider::Name_);
  filter->filter.insert("mojo:resource_provider",
                        std::move(resource_provider_interfaces));

  mojo::Array<mojo::String> network_service_interfaces;
  network_service_interfaces.push_back(mojo::CookieStore::Name_);
  network_service_interfaces.push_back(mojo::NetworkService::Name_);
  network_service_interfaces.push_back(mojo::URLLoaderFactory::Name_);
  network_service_interfaces.push_back(mojo::WebSocketFactory::Name_);
  filter->filter.insert("mojo:network_service",
                        std::move(network_service_interfaces));

  mojo::Array<mojo::String> clipboard_interfaces;
  clipboard_interfaces.push_back(mojo::Clipboard::Name_);
  filter->filter.insert("mojo:clipboard", std::move(clipboard_interfaces));

  mojo::Array<mojo::String> tracing_interfaces;
  tracing_interfaces.push_back(tracing::StartupPerformanceDataCollector::Name_);
  tracing_interfaces.push_back(tracing::TraceCollector::Name_);
  filter->filter.insert("mojo:tracing", std::move(tracing_interfaces));

  mojo::Array<mojo::String> window_manager_interfaces;
  window_manager_interfaces.push_back(mus::mojom::Gpu::Name_);
  window_manager_interfaces.push_back(mus::mojom::WindowTreeHostFactory::Name_);
  window_manager_interfaces.push_back(mus::mojom::DisplayManager::Name_);
  filter->filter.insert("mojo:mus", std::move(window_manager_interfaces));

  mojo::Array<mojo::String> test_runner_interfaces;
  test_runner_interfaces.push_back(LayoutTestRunner::Name_);
  filter->filter.insert("mojo:web_view_test_runner",
                        std::move(test_runner_interfaces));

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  mojo::Array<mojo::String> font_service_interfaces;
  font_service_interfaces.push_back(font_service::FontService::Name_);
  filter->filter.insert("mojo:font_service",
                        std::move(font_service_interfaces));
#endif

  mojo::Shell::ConnectParams params(std::move(request));
  params.set_filter(std::move(filter));
  connection_ = shell->Connect(&params);
  connection_->ConnectToService(&frame_client_);
  connection_->AddRemoteIDCallback(on_got_id_callback);
}

mus::mojom::WindowTreeClientPtr FrameConnection::GetWindowTreeClient() {
  DCHECK(connection_);
  mus::mojom::WindowTreeClientPtr window_tree_client;
  connection_->ConnectToService(&window_tree_client);
  return window_tree_client;
}

uint32_t FrameConnection::GetContentHandlerID() const {
  uint32_t content_handler_id =
      mojo::shell::mojom::Shell::kInvalidApplicationID;
  if (!connection_->GetRemoteContentHandlerID(&content_handler_id))
    NOTREACHED();
  return content_handler_id;
}

}  // namespace web_view
