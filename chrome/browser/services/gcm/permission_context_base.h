// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PERMISSION_CONTEXT_BASE_H_
#define CHROME_BROWSER_SERVICES_GCM_PERMISSION_CONTEXT_BASE_H_

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PermissionQueueController;
class PermissionRequestID;
class Profile;

namespace content {
class WebContents;
}

typedef base::Callback<void(bool)> BrowserPermissionCallback;

// TODO(miguelg): Move this out of gcm into a generic place and make
// Midi permissions and others use it.
namespace gcm {

// This base class contains common operations for granting permissions.
// It is spit out of Midi and Push and will be moved to a common place
// so it can be used by both classes (and eventually others) in a separate
// patch.
// It supports both infobars and bubbles, but it handles them differently.
// For bubbles, it manages the life cycle of permission request and persists the
// permission choices when stated by the user.
// For infobars however all that logic is managed by the internal
// PermissionQueueController object.
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
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        bool user_gesture,
                        const BrowserPermissionCallback& callback);

  // Called when permission is granted without interactively asking the user.
  void PermissionDecided(const PermissionRequestID& id,
                         const GURL& requesting_frame,
                         const GURL& embedder,
                         const BrowserPermissionCallback& callback,
                         bool allowed);

  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_frame,
                           const GURL& embedder,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           bool allowed);

  // Implementors can override this method to update the icons on the
  // url bar with the result of the new permission.
  virtual void UpdateTabContext(const PermissionRequestID& id,
                                const GURL& requesting_frame,
                                bool allowed) {}

  // Return an instance of the infobar queue controller, creating it if needed.
  PermissionQueueController* GetQueueController();

 private:
  void UpdateContentSetting(
      const GURL& requesting_frame,
      const GURL& embedder,
      bool allowed);

  // Called when a bubble is no longer used so it can be cleaned up.
  void CleanUpBubble(const PermissionRequestID& id);

  Profile* profile_;
  const ContentSettingsType permission_type_;
  base::WeakPtrFactory<PermissionContextBase> weak_factory_;
  scoped_ptr<PermissionQueueController> permission_queue_controller_;
  base::ScopedPtrHashMap<std::string, PermissionBubbleRequest>
      pending_bubbles_;
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_PERMISSION_CONTEXT_BASE_H_
