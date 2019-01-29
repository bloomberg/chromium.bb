// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/filtered_service_directory.h"

#include <lib/fdio/util.h>
#include <lib/zx/channel.h>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace base {
namespace fuchsia {

FilteredServiceDirectory::FilteredServiceDirectory(
    const ServiceDirectoryClient* directory)
    : directory_(directory) {
  zx::channel server_channel;
  zx_status_t status =
      zx::channel::create(0, &server_channel, &outgoing_directory_client_);
  ZX_CHECK(status == ZX_OK, status) << "zx_channel_create()";

  outgoing_directory_ =
      std::make_unique<ServiceDirectory>(std::move(server_channel));
}

FilteredServiceDirectory::~FilteredServiceDirectory() {
  outgoing_directory_->RemoveAllServices();
}

void FilteredServiceDirectory::AddService(const char* service_name) {
  outgoing_directory_->AddService(
      service_name,
      base::BindRepeating(&FilteredServiceDirectory::HandleRequest,
                          base::Unretained(this), service_name));
}

zx::channel FilteredServiceDirectory::ConnectClient() {
  zx::channel server_channel;
  zx::channel client_channel;
  zx_status_t status = zx::channel::create(0, &server_channel, &client_channel);
  ZX_CHECK(status == ZX_OK, status) << "zx_channel_create()";

  // ServiceDirectory puts public services under ./public . Connect to that
  // directory and return client handle for the connection,
  status = fdio_service_connect_at(outgoing_directory_client_.get(), "public",
                                   server_channel.release());
  ZX_CHECK(status == ZX_OK, status) << "fdio_service_connect_at()";

  return client_channel;
}

void FilteredServiceDirectory::HandleRequest(const char* service_name,
                                             zx::channel channel) {
  directory_->ConnectToServiceUnsafe(service_name, std::move(channel));
}

}  // namespace fuchsia
}  // namespace base
