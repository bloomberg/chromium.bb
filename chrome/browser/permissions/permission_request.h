// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}

// Used for UMA to record the types of permission prompts shown.
// This corresponds to the PermissionRequestType enum in
// src/tools/metrics/histograms.xml. The usual rules of updating UMA values
// applies to this enum:
// - don't remove values
// - only ever add values at the end
// - keep the PermissionRequestType enum in sync with this definition.
enum class PermissionRequestType {
  UNKNOWN,
  MULTIPLE,
  UNUSED_PERMISSION,
  QUOTA,
  DOWNLOAD,
  MEDIA_STREAM,
  REGISTER_PROTOCOL_HANDLER,
  PERMISSION_GEOLOCATION,
  PERMISSION_MIDI_SYSEX,
  PERMISSION_NOTIFICATIONS,
  PERMISSION_PROTECTED_MEDIA_IDENTIFIER,
  PERMISSION_PUSH_MESSAGING,
  PERMISSION_FLASH,
  // NUM must be the last value in the enum.
  NUM
};

// Used for UMA to record whether a gesture was associated with the request. For
// simplicity not all request types track whether a gesture is associated with
// it or not, for these types of requests metrics are not recorded.
enum class PermissionRequestGestureType {
  UNKNOWN,
  GESTURE,
  NO_GESTURE,
  // NUM must be the last value in the enum.
  NUM
};

// Describes the interface a feature making permission requests should
// implement. A class of this type is registered with the permission request
// manager to receive updates about the result of the permissions request
// from the bubble or infobar. It should live until it is unregistered or until
// RequestFinished is called.
// Note that no particular guarantees are made about what exact UI surface
// is presented to the user. The delegate may be coalesced with other bubble
// requests, or depending on the situation, not shown at all.
class PermissionRequest {
 public:
#if defined(OS_ANDROID)
  // On Android, icons are represented with an IDR_ identifier.
  typedef int IconId;
#else
  // On desktop, we use a vector icon.
  typedef const gfx::VectorIcon& IconId;
#endif

  PermissionRequest();
  virtual ~PermissionRequest() {}

  // The icon to use next to the message text fragment in the permission bubble.
  virtual IconId GetIconId() const = 0;

  // Returns the shortened prompt text for this permission.  Must be phrased
  // as a heading, e.g. "Location", or "Camera". The permission bubble may
  // coalesce different requests, and if it does, this text will be displayed
  // next to an image and indicate the user grants the permission.
  virtual base::string16 GetMessageTextFragment() const = 0;

  // Get the origin on whose behalf this permission request is being made.
  virtual GURL GetOrigin() const = 0;

  // Called when the user has granted the requested permission.
  virtual void PermissionGranted() = 0;

  // Called when the user has denied the requested permission.
  virtual void PermissionDenied() = 0;

  // Called when the user has cancelled the permission request. This
  // corresponds to a denial, but is segregated in case the context needs to
  // be able to distinguish between an active refusal or an implicit refusal.
  virtual void Cancelled() = 0;

  // The UI this request was associated with was answered by the user.
  // It is safe for the request to be deleted at this point -- it will receive
  // no further message from the permission request system. This method will
  // eventually be called on every request which is not unregistered.
  virtual void RequestFinished() = 0;

  // True if a persistence toggle should be shown in the UI for this request.
  virtual bool ShouldShowPersistenceToggle() const;

  // Used to record UMA metrics for permission requests.
  virtual PermissionRequestType GetPermissionRequestType() const;

  // Used to record UMA for whether requests are associated with a user gesture.
  // To keep things simple this metric is only recorded for the most popular
  // request types.
  virtual PermissionRequestGestureType GetGestureType() const;

  // Used on Android to determine what Android OS permissions are needed for
  // this permission request.
  virtual ContentSettingsType GetContentSettingsType() const;

  void set_persist(bool persist) { persist_ = persist; }

 protected:
  bool persist() const { return persist_; }

 private:
  // Whether or not the response for this prompt should be persisted.
  bool persist_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequest);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_H_
