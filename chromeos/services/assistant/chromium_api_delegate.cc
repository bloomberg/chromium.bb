// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/chromium_api_delegate.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace assistant {

ChromiumApiDelegate::ChromiumApiDelegate(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info)
    : http_connection_factory_(std::move(url_loader_factory_info)) {}

ChromiumApiDelegate::~ChromiumApiDelegate() = default;

assistant_client::HttpConnectionFactory*
ChromiumApiDelegate::GetHttpConnectionFactory() {
  return &http_connection_factory_;
}

}  // namespace assistant
}  // namespace chromeos
