// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_

// On 10.14 and above: returns true if permission is not allowed in system
// settings, false otherwise, including if permission is not determined yet.
// On 10.13 and below: returns false, since there are no system media capture
// permissions.
bool SystemAudioCapturePermissionIsDisallowed();
bool SystemVideoCapturePermissionIsDisallowed();

// On 10.14 and above: if system permission is not determined, requests
// permission. Otherwise, does nothing. When requesting permission, the OS will
// show a user dialog and respond asynchronously. This function does not wait
// for the response and nothing is done at the response. The reason
// for explicitly requesting permission is that if only implicitly requesting
// permission (e.g. for audio when media::AUAudioInputStream::Start() calls
// AudioOutputUnitStart()), the OS returns not determined when we ask what the
// permission state is, even though it's actually set to something else, until
// browser restart.
// On 10.13 and below: does nothing, since there are no system media capture
// permissions.
void EnsureSystemAudioCapturePermissionIsOrGetsDetermined();
void EnsureSystemVideoCapturePermissionIsOrGetsDetermined();

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
