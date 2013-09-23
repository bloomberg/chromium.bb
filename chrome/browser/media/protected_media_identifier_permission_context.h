// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"

class PermissionRequestID;
class Profile;

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public base::RefCountedThreadSafe<
        ProtectedMediaIdentifierPermissionContext> {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(Profile* profile);

  void RequestProtectedMediaIdentifierPermission(
      int render_process_id,
      int render_view_id,
      const GURL& requesting_frame,
      const base::Callback<void(bool)>& callback);
  void CancelProtectedMediaIdentifierPermissionRequest(
      int render_process_id,
      int render_view_id,
      const GURL& requesting_frame);

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

  // Notifies whether or not the corresponding bridge is allowed to use
  // protected media identifier via
  // SetProtectedMediaIdentifierPermissionResponse(). Called on the UI thread.
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_frame,
                           const base::Callback<void(bool)>& callback,
                           bool allowed);

  // Decide whether the protected media identifier permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an
  // infobar to the user. Called on the UI thread.
  void DecidePermission(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        const base::Callback<void(bool)>& callback);

  // Called when permission is granted without interactively asking
  // the user. Can be overridden to introduce additional UI flow.
  // Should ultimately ensure that NotifyPermissionSet is called.
  // Called on the UI thread.
  void PermissionDecided(const PermissionRequestID& id,
                         const GURL& requesting_frame,
                         const GURL& embedder,
                         const base::Callback<void(bool)>& callback,
                         bool allowed);

  // Create an PermissionQueueController. overridden in derived classes to
  // provide additional UI flow.  Called on the UI thread.
  PermissionQueueController* CreateQueueController();

  // Removes any pending InfoBar request.
  void CancelPendingInfoBarRequest(const PermissionRequestID& id);

  // These must only be accessed from the UI thread.
  Profile* const profile_;
  bool shutting_down_;
  scoped_ptr<PermissionQueueController> permission_queue_controller_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
