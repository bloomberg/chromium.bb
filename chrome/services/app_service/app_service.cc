// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/app_service.h"

#include <utility>

#include "base/bind.h"

namespace apps {

AppService::AppService() = default;

AppService::~AppService() = default;

void AppService::OnStart() {
  binder_registry_.AddInterface<apps::mojom::AppService>(base::BindRepeating(
      &AppServiceImpl::BindRequest, base::Unretained(&impl_)));
}

void AppService::OnBindInterface(const service_manager::BindSourceInfo& source,
                                 const std::string& interface_name,
                                 mojo::ScopedMessagePipeHandle interface_pipe) {
  binder_registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace apps
