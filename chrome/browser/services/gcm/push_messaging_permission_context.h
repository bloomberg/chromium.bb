// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_H_

#include "chrome/browser/content_settings/permission_context_base.h"

namespace gcm {

// Permission context for push messages.
class PushMessagingPermissionContext : public PermissionContextBase {
 public:
  explicit PushMessagingPermissionContext(Profile* profile);
  virtual ~PushMessagingPermissionContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(PushMessagingPermissionContext);
};

}  // namespace gcm
#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_H_
