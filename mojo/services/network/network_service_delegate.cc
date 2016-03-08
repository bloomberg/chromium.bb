// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_delegate.h"

#include <utility>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/services/network/cookie_store_impl.h"
#include "mojo/services/network/network_service_delegate_observer.h"
#include "mojo/services/network/network_service_impl.h"
#include "mojo/services/network/url_loader_factory_impl.h"
#include "mojo/services/network/web_socket_factory_impl.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/util/capture_util.h"

namespace {

const char kUserDataDir[] = "user-data-dir";

}  // namespace

namespace mojo {

NetworkServiceDelegate::NetworkServiceDelegate() {
  ref_factory_.set_quit_closure(
      base::Bind(&NetworkServiceDelegate::Quit, base::Unretained(this)));
}
NetworkServiceDelegate::~NetworkServiceDelegate() {}

void NetworkServiceDelegate::AddObserver(
    NetworkServiceDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkServiceDelegate::RemoveObserver(
    NetworkServiceDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkServiceDelegate::Initialize(Connector* connector,
                                        const Identity& identity,
                                        uint32_t id) {
  // TODO(erg): Find everything else that writes to the filesystem and
  // transition it to proxying mojo:filesystem. We shouldn't have any path
  // calculation code here, but sadly need it until the transition is done. In
  // the mean time, manually handle the user-data-dir switch (which gets set in
  // tests) so that tests are writing to a temp dir.
  base::FilePath base_path;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserDataDir)) {
    base_path = command_line->GetSwitchValuePath(kUserDataDir);
  } else {
    CHECK(PathService::Get(base::DIR_TEMP, &base_path));
    base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
  }

  context_.reset(new NetworkContext(base_path, this));
  tracing_.Initialize(connector, identity.name());
}

bool NetworkServiceDelegate::AcceptConnection(Connection* connection) {
  DCHECK(context_);
  connection->AddInterface<CookieStore>(this);
  connection->AddInterface<NetworkService>(this);
  connection->AddInterface<URLLoaderFactory>(this);
  connection->AddInterface<WebSocketFactory>(this);
  return true;
}

void NetworkServiceDelegate::Create(Connection* connection,
                                    InterfaceRequest<NetworkService> request) {
  new NetworkServiceImpl(ref_factory_.CreateRef(), std::move(request));
}

void NetworkServiceDelegate::Create(Connection* connection,
                                    InterfaceRequest<CookieStore> request) {
  // TODO(beng): need to find a way to get content origin.
  new CookieStoreImpl(
      context_.get(), GURL(),
      ref_factory_.CreateRef(), std::move(request));
}

void NetworkServiceDelegate::Create(
    Connection* connection,
    InterfaceRequest<WebSocketFactory> request) {
  new WebSocketFactoryImpl(context_.get(), ref_factory_.CreateRef(),
                           std::move(request));
}

void NetworkServiceDelegate::Create(
    Connection* connection,
    InterfaceRequest<URLLoaderFactory> request) {
  new URLLoaderFactoryImpl(context_.get(), ref_factory_.CreateRef(),
                           std::move(request));
}

void NetworkServiceDelegate::Quit() {
  // Destroy the NetworkContext now as it requires MessageLoop::current() upon
  // destruction and it is the last moment we know for sure that it is
  // running.
  context_.reset();
}

}  // namespace mojo
