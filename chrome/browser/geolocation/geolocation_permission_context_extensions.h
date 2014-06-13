// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_

#include "base/callback_forward.h"
#include "base/macros.h"

namespace content {
class WebContents;
}

class GURL;
class PermissionRequestID;
class Profile;

// Chrome extensions specific portions of GeolocationPermissionContext.
class GeolocationPermissionContextExtensions {
 public:
  explicit GeolocationPermissionContextExtensions(Profile* profile);
  ~GeolocationPermissionContextExtensions();

  // Returns true if the permission request was handled. In which case,
  // |permission_set| will be set to true if the permission changed, and the
  // permission has been set to |new_permission|.
  bool RequestPermission(content::WebContents* web_contents,
                         const PermissionRequestID& request_id,
                         int bridge_id,
                         const GURL& requesting_frame,
                         bool user_gesture,
                         base::Callback<void(bool)> callback,
                         bool* permission_set,
                         bool* new_permission);

  // Returns true if the cancellation request was handled.
  bool CancelPermissionRequest(content::WebContents* web_contents,
                               int bridge_id);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextExtensions);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_
