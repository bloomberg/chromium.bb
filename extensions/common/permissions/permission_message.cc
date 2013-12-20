// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message.h"

namespace extensions {
//
// PermissionMessage
//

PermissionMessage::PermissionMessage(
    PermissionMessage::ID id, const base::string16& message)
  : id_(id),
    message_(message) {
}

PermissionMessage::PermissionMessage(
    PermissionMessage::ID id,
    const base::string16& message,
    const base::string16& details)
  : id_(id),
    message_(message),
    details_(details) {
}

PermissionMessage::~PermissionMessage() {}

}  // namespace extensions
