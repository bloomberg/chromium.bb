// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_V4L2_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_V4L2_WEBCAM_H_

#include "extensions/browser/api/webcam_private/webcam.h"

#include "base/files/scoped_file.h"

namespace extensions {

class V4L2Webcam : public Webcam {
 public:
  V4L2Webcam(const std::string& device_id);
  bool Open();

 private:
  ~V4L2Webcam() override;
  bool EnsureLogitechCommandsMapped();
  bool SetWebcamParameter(int fd, uint32_t control_id, int value);
  bool GetWebcamParameter(int fd, uint32_t control_id, int* value);

  // Webcam:
  void Reset(bool pan, bool tilt, bool zoom) override;
  bool GetPan(int* value) override;
  bool GetTilt(int* value) override;
  bool GetZoom(int* value) override;
  bool SetPan(int value) override;
  bool SetTilt(int value) override;
  bool SetZoom(int value) override;
  bool SetPanDirection(PanDirection direction) override;
  bool SetTiltDirection(TiltDirection direction) override;

  const std::string device_id_;
  base::ScopedFD fd_;

  DISALLOW_COPY_AND_ASSIGN(V4L2Webcam);
};


}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_V4L2_WEBCAM_H_
