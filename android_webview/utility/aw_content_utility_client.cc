// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/utility/aw_content_utility_client.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace android_webview {

AwContentUtilityClient::AwContentUtilityClient() = default;
AwContentUtilityClient::~AwContentUtilityClient() = default;

void AwContentUtilityClient::UtilityThreadStarted() {
  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();
  DCHECK(connection);

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  connection->AddConnectionFilter(
      std::make_unique<content::SimpleConnectionFilter>(std::move(registry)));
}

}  // namespace android_webview
