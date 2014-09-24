// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_CONTEXT_BASE_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_CONTEXT_BASE_H_

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PermissionQueueController;
class PermissionRequestID;
class Profile;

namespace content {
class WebContents;
}

typedef base::Callback<void(bool)> BrowserPermissionCallback;

// This base class contains common operations for granting permissions.
// It offers the following functionality:
//   - Creates a bubble or infobar when a permission is needed
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
//   - Edit the PermissionBubbleRequestImpl methods to add the new text for
//     the bubble.
//   - Hit several asserts for the missing plumbing and fix them :)
// After this you can override several other methods to customize behavior,
// in particular it is advised to override UpdateTabContext in order to manage
// the permission from the omnibox.
// See midi_permission_context.h/cc or push_permission_context.cc/h for some
// examples.

class PermissionContextBase : public KeyedService {
 public:
  PermissionContextBase(Profile* profile,
                        const ContentSettingsType permission_type);
  virtual ~PermissionContextBase();

  // The renderer is requesting permission to push messages.
  // When the answer to a permission request has been determined, |callback|
  // should be called with the result.
  virtual void RequestPermission(content::WebContents* web_contents,
                                 const PermissionRequestID& id,
                                 const GURL& requesting_frame,
                                 bool user_gesture,
                                 const BrowserPermissionCallback& callback);

 protected:
  // Decide whether the permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an infobar.
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedder_origin,
                        bool user_gesture,
                        const BrowserPermissionCallback& callback);

  // Called when permission is granted without interactively asking the user.
  void PermissionDecided(const PermissionRequestID& id,
                         const GURL& requesting_origin,
                         const GURL& embedder_origin,
                         const BrowserPermissionCallback& callback,
                         bool persist,
                         bool allowed);

  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           bool allowed);

  // Implementors can override this method to update the icons on the
  // url bar with the result of the new permission.
  virtual void UpdateTabContext(const PermissionRequestID& id,
                                const GURL& requesting_origin,
                                bool allowed) {}

  // Return an instance of the infobar queue controller, creating it if needed.
  PermissionQueueController* GetQueueController();

  // Store the decided permission as a content setting.
  // virtual since the permission might be stored with different restrictions
  // (for example for desktop notifications).
  virtual void UpdateContentSetting(const GURL& requesting_origin,
                                    const GURL& embedder_origin,
                                    bool allowed);

 private:

  // Called when a bubble is no longer used so it can be cleaned up.
  void CleanUpBubble(const PermissionRequestID& id);

  Profile* profile_;
  const ContentSettingsType permission_type_;
  scoped_ptr<PermissionQueueController> permission_queue_controller_;
  base::ScopedPtrHashMap<std::string, PermissionBubbleRequest>
      pending_bubbles_;

  base::WeakPtrFactory<PermissionContextBase> weak_factory_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_CONTEXT_BASE_H_
