// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"


// Android-specific geolocation permission flow, taking into account the
// Google Location Settings, if available.
class ChromeGeolocationPermissionContextAndroid
    : public ChromeGeolocationPermissionContext {
 public:
  explicit ChromeGeolocationPermissionContextAndroid(Profile* profile);

 private:
  friend class ChromeGeolocationPermissionContext;

  // ChromeGeolocationPermissionContext implementation:
  virtual void DecidePermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      const GURL& embedder,
      base::Callback<void(bool)> callback) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeGeolocationPermissionContextAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
