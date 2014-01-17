// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_DELEGATE_H_

#include "base/strings/string16.h"

// Describes the interface a feature utilizing permission bubbles should
// implement. A class of this type is registered with the permission bubble
// manager to receive updates about the result of the permissions request
// from the bubble. It should live until it is unregistered or until
// PermissionsBubbleDestroyed is called.
// Note that no particular guarantees are made about what exact UI surface
// is presented to the user. The delegate may be coalesced with other bubble
// requests, or depending on the situation, not shown at all.
class PermissionBubbleDelegate {
 public:
  virtual ~PermissionBubbleDelegate() {}

  // Returns the resource ID of an associated icon. If kNoIconID is returned, no
  // icon will be shown.
  virtual int GetIconID() const = 0;

  // Returns the prompt text for this permission. This is the central permission
  // grant text, and must be phrased positively -- the permission bubble may
  // coalesce different requests, and if it does, this text will be displayed
  // next to a checkbox indicating the user grants the permission.
  virtual base::string16 GetMessageText() const = 0;

  // TODO(gbillock): Needed?
  // Returns alternative text for the accept button in the case where this
  // single permission request is triggered in the bubble.
  // If the permission request is coalesced, the text will revert to the default
  // "Accept"-alike, so the message text must be clear enough for users to
  // understand even if this text is not used.
  virtual base::string16 GetAlternateAcceptButtonText() const = 0;

  // Called when the user has granted the requested permission.
  virtual void PermissionGranted() = 0;

  // Called when the user has denied the requested permission.
  virtual void PermissionDenied() = 0;

  // Called when the user has cancelled the permission request. This
  // corresponds to a denial, but is segregated in case the context needs to
  // be able to distinguish between an active refusal or an implicit refusal.
  virtual void Cancelled() = 0;

  // The bubble this delegate was associated with was answered by the user.
  // It is safe for the delegate to be deleted at this point -- it will receive
  // no further message from the permission bubble system. This method will
  // eventually be called on every delegate which is not unregistered.
  virtual void RequestFinished() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_DELEGATE_H_
