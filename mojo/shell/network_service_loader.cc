// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/network_service_loader.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/network/network_service_impl.h"

namespace {
base::FilePath GetBasePath() {
  base::FilePath path;
  CHECK(PathService::Get(base::DIR_TEMP, &path));
  return path.Append(FILE_PATH_LITERAL("network_service"));
}
}

namespace mojo {
namespace shell {

NetworkServiceLoader::NetworkServiceLoader() {
}

NetworkServiceLoader::~NetworkServiceLoader() {
}

void NetworkServiceLoader::LoadService(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle shell_handle) {
  uintptr_t key = reinterpret_cast<uintptr_t>(manager);
  if (apps_.find(key) == apps_.end()) {
    scoped_ptr<ApplicationImpl> app(
        new ApplicationImpl(this, shell_handle.Pass()));
    apps_.add(key, app.Pass());
  }
}

void NetworkServiceLoader::OnServiceError(ServiceManager* manager,
                                          const GURL& url) {
  apps_.erase(reinterpret_cast<uintptr_t>(manager));
}

void NetworkServiceLoader::Initialize(ApplicationImpl* app) {
  // The context must be created on the same thread as the network service.
  context_.reset(new NetworkContext(GetBasePath()));
}

bool NetworkServiceLoader::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<NetworkServiceImpl>(context_.get());
  return true;
}

}  // namespace shell
}  // namespace mojo
