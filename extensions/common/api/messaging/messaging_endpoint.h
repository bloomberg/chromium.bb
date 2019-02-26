// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_

#include <string>

#include "base/optional.h"
#include "extensions/common/extension_id.h"

namespace extensions {

struct MessagingEndpoint {
  // Type of the messaging source or destination - i.e., the type of the
  // component which talks to a messaging channel.
  enum class Type {
    // An extension.
    kExtension = 0,
    // A web page or a content script or a hosted app.
    kTab = 1,
    // A native application.
    kNativeApp = 2,

    // This item must be equal to the last actual enum item.
    kLast = kNativeApp,
  };

  static MessagingEndpoint ForExtension(ExtensionId extension_id);
  static MessagingEndpoint ForContentScript(ExtensionId extension_id);
  static MessagingEndpoint ForWebPage();
  static MessagingEndpoint ForNativeApp(std::string native_app_name);

  MessagingEndpoint();
  MessagingEndpoint(const MessagingEndpoint&);
  MessagingEndpoint(MessagingEndpoint&&);

  MessagingEndpoint& operator=(const MessagingEndpoint&);

  ~MessagingEndpoint();

  Type type = Type::kExtension;

  // Identifier of the extension (or the content script).  It is required for
  // |type| of kExtension.  For |type| of kTab, it is set if the endpoint is a
  // content script (otherwise, it's the web page).
  base::Optional<ExtensionId> extension_id;

  // Name of the native application.  It is required for |type| of kNativeApp.
  // It is not used for other types.
  base::Optional<std::string> native_app_name;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_
