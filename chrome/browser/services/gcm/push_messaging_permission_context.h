// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_H_

#include "chrome/browser/content_settings/permission_context_base.h"

#include "components/content_settings/core/common/content_settings_types.h"

class PermissionRequestID;
class Profile;

namespace gcm {

// Permission context for push messages.
class PushMessagingPermissionContext : public PermissionContextBase {
 public:
  explicit PushMessagingPermissionContext(Profile* profile);
  ~PushMessagingPermissionContext() override;

  // PermissionContextBase:
  ContentSetting GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  void CancelPermissionRequest(content::WebContents* web_contents,
                               const PermissionRequestID& id) override;

 protected:
  // PermissionContextBase:
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        const BrowserPermissionCallback& callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PushMessagingPermissionContextTest,
                           DecidePushPermission);

  // Used to decide the permission for push, once the permission for
  // Notification has been granted/denied.
  void DecidePushPermission(const PermissionRequestID& id,
                            const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            const BrowserPermissionCallback& callback,
                            ContentSetting notifications_content_setting);

  Profile* profile_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<PushMessagingPermissionContext> weak_factory_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingPermissionContext);
};

}  // namespace gcm
#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_H_
