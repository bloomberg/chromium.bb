// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_

#include <set>
#include <string>

#include "base/macros.h"

namespace extensions {

class Webcam {
 public:
  enum PanDirection {
    PAN_LEFT,
    PAN_RIGHT,
    PAN_STOP,
  };

  enum TiltDirection {
    TILT_UP,
    TILT_DOWN,
    TILT_STOP,
  };

  Webcam();
  virtual ~Webcam();

  virtual void Reset(bool pan, bool tilt, bool zoom) = 0;
  virtual bool GetPan(int* value) = 0;
  virtual bool GetTilt(int* value) = 0;
  virtual bool GetZoom(int* value) = 0;
  virtual bool SetPan(int value) = 0;
  virtual bool SetTilt(int value) = 0;
  virtual bool SetZoom(int value) = 0;
  virtual bool SetPanDirection(PanDirection direction) = 0;
  virtual bool SetTiltDirection(TiltDirection direction) = 0;

  void AddExtensionRef(const std::string& extension_id) {
    extension_refs_.insert(extension_id);
  }

  void RemoveExtensionRef(const std::string& extension_id) {
    extension_refs_.erase(extension_id);
  }

  bool ShouldDelete() {
    return extension_refs_.empty();
  }

 private:
  std::set<std::string> extension_refs_;

  DISALLOW_COPY_AND_ASSIGN(Webcam);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_
