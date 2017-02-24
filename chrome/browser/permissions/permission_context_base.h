// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_

#include <memory>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_result.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"

#if defined(OS_ANDROID)
class PermissionQueueController;
#endif
class GURL;
class PermissionRequestID;
class Profile;

namespace content {
class WebContents;
}

using BrowserPermissionCallback = base::Callback<void(ContentSetting)>;

// This base class contains common operations for granting permissions.
// It offers the following functionality:
//   - Creates a permission request when needed.
//   - If accepted/denied the permission is saved in content settings for
//     future uses (for the domain that requested it).
//   - If dismissed the permission is not saved but it's considered denied for
//     this one request
//   - In any case the BrowserPermissionCallback is executed once a decision
//     about the permission is made by the user.
// The bare minimum you need to create a new permission request is
//   - Define your new permission in the ContentSettingsType enum.
//   - Create a class that inherits from PermissionContextBase and passes the
//     new permission.
//   - Inherit from PermissionInfobarDelegate and implement
//     |GetMessageText|
//   - Edit the PermissionRequestImpl methods to add the new text.
//   - Hit several asserts for the missing plumbing and fix them :)
// After this you can override several other methods to customize behavior,
// in particular it is advised to override UpdateTabContext in order to manage
// the permission from the omnibox.
// It is mandatory to override IsRestrictedToSecureOrigin.
// See midi_permission_context.h/cc or push_permission_context.cc/h for some
// examples.

class PermissionContextBase : public KeyedService {
 public:
  PermissionContextBase(Profile* profile,
                        const ContentSettingsType content_settings_type);
  ~PermissionContextBase() override;

  // A field trial used to enable the global permissions kill switch.
  // This is public so permissions that don't yet inherit from
  // PermissionContextBase can use it.
  static const char kPermissionsKillSwitchFieldStudy[];

  // The field trial param to enable the global permissions kill switch.
  // This is public so permissions that don't yet inherit from
  // PermissionContextBase can use it.
  static const char kPermissionsKillSwitchBlockedValue[];

  // The renderer is requesting permission to push messages.
  // When the answer to a permission request has been determined, |callback|
  // should be called with the result.
  virtual void RequestPermission(content::WebContents* web_contents,
                                 const PermissionRequestID& id,
                                 const GURL& requesting_frame,
                                 bool user_gesture,
                                 const BrowserPermissionCallback& callback);

  // Returns whether the permission has been granted, denied etc.
  // TODO(meredithl): Ensure that the result accurately reflects whether the
  // origin is blacklisted for this permission.
  PermissionResult GetPermissionStatus(const GURL& requesting_origin,
                                       const GURL& embedding_origin) const;

  // Resets the permission to its default value.
  virtual void ResetPermission(const GURL& requesting_origin,
                               const GURL& embedding_origin);

  // Withdraw an existing permission request, no op if the permission request
  // was already cancelled by some other means.
  virtual void CancelPermissionRequest(content::WebContents* web_contents,
                                       const PermissionRequestID& id);

  // Whether the kill switch has been enabled for this permission.
  // public for permissions that do not use RequestPermission, like
  // camera and microphone, and for testing.
  bool IsPermissionKillSwitchOn() const;

 protected:
  virtual ContentSetting GetPermissionStatusInternal(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const;

  // Decide whether the permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an infobar.
  virtual void DecidePermission(content::WebContents* web_contents,
                                const PermissionRequestID& id,
                                const GURL& requesting_origin,
                                const GURL& embedding_origin,
                                bool user_gesture,
                                const BrowserPermissionCallback& callback);

  // Called when permission is granted without interactively asking the user.
  void PermissionDecided(const PermissionRequestID& id,
                         const GURL& requesting_origin,
                         const GURL& embedding_origin,
                         bool user_gesture,
                         const BrowserPermissionCallback& callback,
                         bool persist,
                         ContentSetting content_setting);

  virtual void NotifyPermissionSet(const PermissionRequestID& id,
                                   const GURL& requesting_origin,
                                   const GURL& embedding_origin,
                                   const BrowserPermissionCallback& callback,
                                   bool persist,
                                   ContentSetting content_setting);

  // Implementors can override this method to update the icons on the
  // url bar with the result of the new permission.
  virtual void UpdateTabContext(const PermissionRequestID& id,
                                const GURL& requesting_origin,
                                bool allowed) {}

#if defined(OS_ANDROID)
  // Return an instance of the infobar queue controller, creating it if needed.
  PermissionQueueController* GetQueueController();
#endif

  // Returns the profile associated with this permission context.
  Profile* profile() const;

  // Store the decided permission as a content setting.
  // virtual since the permission might be stored with different restrictions
  // (for example for desktop notifications).
  virtual void UpdateContentSetting(const GURL& requesting_origin,
                                    const GURL& embedding_origin,
                                    ContentSetting content_setting);

  // Whether the permission should be restricted to secure origins.
  virtual bool IsRestrictedToSecureOrigins() const = 0;

  ContentSettingsType content_settings_type() const {
    return content_settings_type_;
  }

  // TODO(timloh): The CONTENT_SETTINGS_TYPE_NOTIFICATIONS type is used to
  // store both push messaging and notifications permissions. Remove this
  // once we've unified these types (crbug.com/563297).
  ContentSettingsType content_settings_storage_type() const;

 private:
  friend class PermissionContextBaseTests;

  // Called when a request is no longer used so it can be cleaned up.
  void CleanUpRequest(const PermissionRequestID& id);

  // Called when the requesting origin and permission have been checked by Safe
  // Browsing. |permission_blocked| determines whether to auto-block the
  // permission request without prompting the user for a decision.
  void ContinueRequestPermission(content::WebContents* web_contents,
                                 const PermissionRequestID& id,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin,
                                 bool user_gesture,
                                 const BrowserPermissionCallback& callback,
                                 bool permission_blocked);

  Profile* profile_;
  const ContentSettingsType content_settings_type_;
#if defined(OS_ANDROID)
  std::unique_ptr<PermissionQueueController> permission_queue_controller_;
#endif
  std::unordered_map<std::string, std::unique_ptr<PermissionRequest>>
      pending_requests_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<PermissionContextBase> weak_factory_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_
