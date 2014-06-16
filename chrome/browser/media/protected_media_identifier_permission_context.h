// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"

class PermissionRequestID;
class Profile;

namespace content {
class RenderViewHost;
class WebContents;
}

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public base::RefCountedThreadSafe<
        ProtectedMediaIdentifierPermissionContext> {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(Profile* profile);

  void RequestProtectedMediaIdentifierPermission(
      content::WebContents* web_contents,
      const GURL& origin,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback);

  // Called on the UI thread when the profile is about to be destroyed.
  void ShutdownOnUIThread();

 private:
  friend class base::RefCountedThreadSafe<
      ProtectedMediaIdentifierPermissionContext>;
  ~ProtectedMediaIdentifierPermissionContext();

  Profile* profile() const { return profile_; }

  // Return an instance of the infobar queue controller, creating it
  // if necessary.
  PermissionQueueController* QueueController();

  void CancelProtectedMediaIdentifierPermissionRequests(
      int render_process_id,
      int render_view_id,
      const GURL& origin);

  // Notifies whether or not the corresponding bridge is allowed to use
  // protected media identifier via
  // SetProtectedMediaIdentifierPermissionResponse(). Called on the UI thread.
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& origin,
                           const base::Callback<void(bool)>& callback,
                           bool allowed);

  // Decide whether the protected media identifier permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an
  // infobar to the user. Called on the UI thread.
  void DecidePermission(const PermissionRequestID& id,
                        const GURL& origin,
                        const GURL& embedder,
                        content::RenderViewHost* rvh,
                        const base::Callback<void(bool)>& callback);

  // Called when permission is granted without interactively asking
  // the user. Can be overridden to introduce additional UI flow.
  // Should ultimately ensure that NotifyPermissionSet is called.
  // Called on the UI thread.
  void PermissionDecided(const PermissionRequestID& id,
                         const GURL& origin,
                         const GURL& embedder,
                         const base::Callback<void(bool)>& callback,
                         bool allowed);

  // Create an PermissionQueueController. overridden in derived classes to
  // provide additional UI flow.  Called on the UI thread.
  PermissionQueueController* CreateQueueController();

  // Removes pending InfoBar requests that match |bridge_id| from the tab
  // given by |render_process_id| and |render_view_id|.
  void CancelPendingInfobarRequests(int render_process_id,
                                    int render_view_id,
                                    const GURL& origin);

  // These must only be accessed from the UI thread.
  Profile* const profile_;
  bool shutting_down_;
  scoped_ptr<PermissionQueueController> permission_queue_controller_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
