// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_REQUEST_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_REQUEST_H_

#include "base/strings/string16.h"

// Describes the interface a feature utilizing permission bubbles should
// implement. A class of this type is registered with the permission bubble
// manager to receive updates about the result of the permissions request
// from the bubble. It should live until it is unregistered or until
// PermissionsBubbleDestroyed is called.
// Note that no particular guarantees are made about what exact UI surface
// is presented to the user. The delegate may be coalesced with other bubble
// requests, or depending on the situation, not shown at all.
class PermissionBubbleRequest {
 public:
  virtual ~PermissionBubbleRequest() {}

  // Returns the full prompt text for this permission. This is the only text
  // that will be shown in the single-permission case and should be phrased
  // positively as a complete sentence.
  virtual base::string16 GetMessageText() const = 0;

  // Returns the shortened prompt text for this permission.  Must be phrased
  // positively -- the permission bubble may coalesce different requests, and
  // if it does, this text will be displayed next to a bullet or checkbox
  // indicating the user grants the permission.
  virtual base::string16 GetMessageTextFragment() const = 0;

  // May return alternative text for the accept button in the case where this
  // single permission request is triggered in the bubble. If it returns an
  // empty string the default is used.
  // If the permission request is coalesced, the text will revert to the default
  // "Accept"-alike, so the message text must be clear enough for users to
  // understand even if this text is not used.
  virtual base::string16 GetAlternateAcceptButtonText() const = 0;

  // May return alternative text for the deny button in the case where this
  // single permission request is triggered in the bubble. If it returns an
  // empty string the default is used. This text may not be used at all,
  // so the |GetMessageText()| prompt should be clear enough to convey the
  // permission request with generic button text.
  virtual base::string16 GetAlternateDenyButtonText() const = 0;

  // Called when the user has granted the requested permission.
  virtual void PermissionGranted() = 0;

  // Called when the user has denied the requested permission.
  virtual void PermissionDenied() = 0;

  // Called when the user has cancelled the permission request. This
  // corresponds to a denial, but is segregated in case the context needs to
  // be able to distinguish between an active refusal or an implicit refusal.
  virtual void Cancelled() = 0;

  // The bubble this request was associated with was answered by the user.
  // It is safe for the request to be deleted at this point -- it will receive
  // no further message from the permission bubble system. This method will
  // eventually be called on every request which is not unregistered.
  virtual void RequestFinished() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_REQUEST_H_
