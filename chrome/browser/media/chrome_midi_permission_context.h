// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_H_

#include "base/memory/scoped_ptr.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/browser_context.h"

class GURL;
class PermissionQueueController;
class PermissionRequestID;
class Profile;

// This class manages MIDI permissions flow. Used on the UI thread.
class ChromeMIDIPermissionContext : public BrowserContextKeyedService {
 public:
  explicit ChromeMIDIPermissionContext(Profile* profile);
  virtual ~ChromeMIDIPermissionContext();

  // BrowserContextKeyedService methods:
  virtual void Shutdown() OVERRIDE;

  // Request to ask users permission about MIDI.
  void RequestMIDISysExPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      const content::BrowserContext::MIDISysExPermissionCallback& callback);

  // Cancel a pending MIDI permission request.
  void CancelMIDISysExPermissionRequest(int render_process_id,
                                        int render_view_id,
                                        int bridge_id,
                                        const GURL& requesting_frame);

 private:
  // Decide whether the permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an infobar.
  void DecidePermission(
      const PermissionRequestID& id,
      const GURL& requesting_frame,
      const GURL& embedder,
      const content::BrowserContext::MIDISysExPermissionCallback& callback);

  // Called when permission is granted without interactively asking the user.
  void PermissionDecided(
      const PermissionRequestID& id,
      const GURL& requesting_frame,
      const GURL& embedder,
      const content::BrowserContext::MIDISysExPermissionCallback& callback,
      bool allowed);

  // Called when the permission decision is made. It may be by the
  // InfoBarDelegate to notify permission has been set.
  void NotifyPermissionSet(
      const PermissionRequestID& id,
      const GURL& requesting_frame,
      const content::BrowserContext::MIDISysExPermissionCallback& callback,
      bool allowed);

  // Return an instance of the infobar queue controller, creating it if needed.
  PermissionQueueController* GetQueueController();

  // Removes any pending InfoBar request.
  void CancelPendingInfoBarRequest(const PermissionRequestID& id);

  Profile* const profile_;
  bool shutting_down_;
  scoped_ptr<PermissionQueueController> permission_queue_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMIDIPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_H_
