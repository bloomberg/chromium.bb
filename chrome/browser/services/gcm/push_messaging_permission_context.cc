// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_permission_context.h"

namespace gcm {

PushMessagingPermissionContext::PushMessagingPermissionContext(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING) {
}

PushMessagingPermissionContext::~PushMessagingPermissionContext() {
}

}  // namespace gcm
