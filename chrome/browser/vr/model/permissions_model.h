// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_PERMISSIONS_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_PERMISSIONS_MODEL_H_

namespace vr {

struct PermissionsModel {
  bool audio_capture_enabled = false;
  bool video_capture_enabled = false;
  bool screen_capture_enabled = false;
  bool location_access = false;
  bool bluetooth_connected = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_PERMISSIONS_MODEL_H_
