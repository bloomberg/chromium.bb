// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_PERMISSION_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

class PermissionRequestID;
class Profile;

// Permission context for push messages.
class PushMessagingPermissionContext : public PermissionContextBase {
 public:
  explicit PushMessagingPermissionContext(Profile* profile);
  ~PushMessagingPermissionContext() override;

  // PermissionContextBase:
  ContentSetting GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 protected:
  // PermissionContextBase:
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        const BrowserPermissionCallback& callback) override;
  bool IsRestrictedToSecureOrigins() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PushMessagingPermissionContextTest,
                           DecidePermission);
  FRIEND_TEST_ALL_PREFIXES(PushMessagingPermissionContextTest,
                           DecidePushPermission);
  FRIEND_TEST_ALL_PREFIXES(PushMessagingPermissionContextTest, InsecureOrigin);

  // Used to decide the permission for push, once the permission for
  // Notification has been granted/denied.
  void DecidePushPermission(
      const PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const BrowserPermissionCallback& callback,
      blink::mojom::PermissionStatus notifications_status);

  Profile* profile_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<PushMessagingPermissionContext> weak_factory_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingPermissionContext);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_PERMISSION_CONTEXT_H_
