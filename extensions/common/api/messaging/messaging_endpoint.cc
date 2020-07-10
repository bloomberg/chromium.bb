// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/messaging_endpoint.h"

#include <utility>

namespace extensions {

// static
MessagingEndpoint MessagingEndpoint::ForExtension(ExtensionId extension_id) {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kExtension;
  messaging_endpoint.extension_id = std::move(extension_id);
  return messaging_endpoint;
}

// static
MessagingEndpoint MessagingEndpoint::ForContentScript(
    ExtensionId extension_id) {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kTab;
  messaging_endpoint.extension_id = std::move(extension_id);
  return messaging_endpoint;
}

// static
MessagingEndpoint MessagingEndpoint::ForWebPage() {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kTab;
  return messaging_endpoint;
}

// static
MessagingEndpoint MessagingEndpoint::ForNativeApp(std::string native_app_name) {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kNativeApp;
  messaging_endpoint.native_app_name = std::move(native_app_name);
  return messaging_endpoint;
}

MessagingEndpoint::MessagingEndpoint() = default;

MessagingEndpoint::MessagingEndpoint(const MessagingEndpoint&) = default;

MessagingEndpoint::MessagingEndpoint(MessagingEndpoint&&) = default;

MessagingEndpoint& MessagingEndpoint::operator=(const MessagingEndpoint&) =
    default;

MessagingEndpoint::~MessagingEndpoint() = default;

}  // namespace extensions
