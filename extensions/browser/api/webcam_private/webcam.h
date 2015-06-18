// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"

namespace extensions {

class Webcam : public base::RefCounted<Webcam> {
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

  virtual void Reset(bool pan, bool tilt, bool zoom) = 0;
  virtual bool GetPan(int* value) = 0;
  virtual bool GetTilt(int* value) = 0;
  virtual bool GetZoom(int* value) = 0;
  virtual bool SetPan(int value) = 0;
  virtual bool SetTilt(int value) = 0;
  virtual bool SetZoom(int value) = 0;
  virtual bool SetPanDirection(PanDirection direction) = 0;
  virtual bool SetTiltDirection(TiltDirection direction) = 0;

 protected:
  friend class base::RefCounted<Webcam>;
  virtual ~Webcam();

 private:
  DISALLOW_COPY_AND_ASSIGN(Webcam);
};

class WebcamResource : public ApiResource {
 public:
  WebcamResource(const std::string& owner_extension_id,
                 Webcam* webcam,
                 const std::string& webcam_id);
  ~WebcamResource() override;

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

  Webcam* GetWebcam() const;
  const std::string GetWebcamId() const;

  // ApiResource overrides.
  bool IsPersistent() const override;

 private:
  scoped_refptr<Webcam> webcam_;
  std::string webcam_id_;

  DISALLOW_COPY_AND_ASSIGN(WebcamResource);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_
