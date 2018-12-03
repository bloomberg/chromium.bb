// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_factory.h"

#include <utility>

#include "base/bind.h"
#include "content/public/common/content_client.h"

namespace content {

ServiceFactory::ServiceFactory() = default;

ServiceFactory::~ServiceFactory() = default;

bool ServiceFactory::HandleServiceRequest(
    const std::string& name,
    service_manager::mojom::ServiceRequest request) {
  return false;
}

void ServiceFactory::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& name,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  if (HandleServiceRequest(name, std::move(request)))
    return;

  // DCHECK in developer builds to make these errors easier to identify.
  // Otherwise they result only in cryptic browser error messages.
  NOTREACHED() << "Unable to start service \"" << name << "\". Did you "
               << "forget to register the service in the utility process's"
               << "ServiceFactory?";
  OnLoadFailed();
}

}  // namespace content
