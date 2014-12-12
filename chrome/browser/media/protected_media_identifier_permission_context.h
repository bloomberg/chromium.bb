// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include "chrome/browser/content_settings/permission_context_base.h"

class PermissionRequestID;
class Profile;

namespace content {
class RenderViewHost;
class WebContents;
}

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public PermissionContextBase {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(Profile* profile);

  // In addition to the base class flow checks that it is only code from
  // valid iframes. It also adds special logic when called through an extension.
  void RequestPermission(content::WebContents* web_contents,
                         const PermissionRequestID& id,
                         const GURL& requesting_frame_origin,
                         bool user_gesture,
                         const BrowserPermissionCallback& callback) override;

 private:
  ~ProtectedMediaIdentifierPermissionContext() override;

  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
