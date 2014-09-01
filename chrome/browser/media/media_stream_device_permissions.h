// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICE_PERMISSIONS_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICE_PERMISSIONS_H_

class GURL;
class Profile;

enum MediaStreamDevicePolicy {
  POLICY_NOT_SET,
  ALWAYS_DENY,
  ALWAYS_ALLOW,
};

// Returns true if security origin is from internal objects like
// chrome://URLs, otherwise returns false.
bool CheckAllowAllMediaStreamContentForOrigin(Profile* profile,
                                              const GURL& security_origin);

// Get the device policy for |security_origin| and |profile|.
MediaStreamDevicePolicy GetDevicePolicy(Profile* profile,
                                        const GURL& security_origin,
                                        const char* policy_name,
                                        const char* whitelist_policy_name);

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICE_PERMISSIONS_H_
