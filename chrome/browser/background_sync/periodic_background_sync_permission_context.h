// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "components/content_settings/core/common/content_settings.h"

class Profile;

// This permission context is responsible for getting, deciding on and updating
// the Periodic Background Sync permission for a particular website. This
// permission guards the use of the Periodic Background Sync API. It's not being
// persisted because it's dynamic and relies on either the presence of a PWA for
// the origin, and the Periodic and one-shot Background Sync content settings.
// The user is never prompted for this permission. They can disable the feature
// by disabling the (one-shot) Background Sync permission from content settings
// UI. The periodic Background Sync content setting can be disabled via Finch,
// and will prevent usage of the API.
class PeriodicBackgroundSyncPermissionContext : public PermissionContextBase {
 public:
  explicit PeriodicBackgroundSyncPermissionContext(Profile* profile);
  ~PeriodicBackgroundSyncPermissionContext() override = default;

 protected:
  // Virtual for testing.
  virtual bool IsPwaInstalled(const GURL& url) const;

 private:
  // PermissionContextBase implementation.
  bool IsRestrictedToSecureOrigins() const override;
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override;
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting) override;

  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncPermissionContext);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
