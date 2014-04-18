// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_REQUEST_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_REQUEST_H_

#include "base/strings/string16.h"
#include "url/gurl.h"

// Describes the interface a feature utilizing permission bubbles should
// implement. A class of this type is registered with the permission bubble
// manager to receive updates about the result of the permissions request
// from the bubble. It should live until it is unregistered or until
// RequestFinished is called.
// Note that no particular guarantees are made about what exact UI surface
// is presented to the user. The delegate may be coalesced with other bubble
// requests, or depending on the situation, not shown at all.
class PermissionBubbleRequest {
 public:
  virtual ~PermissionBubbleRequest() {}

  // The icon to use next to the message text fragment in the permission bubble.
  // Must be a valid icon of size 16x16. (TODO(gbillock): tbd size)
  virtual int GetIconID() const = 0;

  // Returns the full prompt text for this permission. This is the only text
  // that will be shown in the single-permission case and should be phrased
  // positively as a complete sentence.
  virtual base::string16 GetMessageText() const = 0;

  // Returns the shortened prompt text for this permission.  Must be phrased
  // as a heading, e.g. "Location", or "Camera". The permission bubble may
  // coalesce different requests, and if it does, this text will be displayed
  // next to an image and indicate the user grants the permission.
  virtual base::string16 GetMessageTextFragment() const = 0;

  // Get whether this request was accompanied by a user gesture. User gestured
  // permissions requests will not be suppressed.
  virtual bool HasUserGesture() const = 0;

  // Get the hostname on whose behalf this permission request is being made.
  virtual GURL GetRequestingHostname() const = 0;

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
