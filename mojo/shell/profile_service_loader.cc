// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/profile_service_loader.h"

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/profile/profile_service_impl.h"

namespace mojo {
namespace shell {

ProfileServiceLoader::ProfileServiceLoader() {
}

ProfileServiceLoader::~ProfileServiceLoader() {
}

void ProfileServiceLoader::LoadService(
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

void ProfileServiceLoader::OnServiceError(ServiceManager* manager,
                                          const GURL& url) {
  apps_.erase(reinterpret_cast<uintptr_t>(manager));
}

bool ProfileServiceLoader::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
    connection->AddService<ProfileServiceImpl>();
  return true;
}

}  // namespace shell
}  // namespace mojo
