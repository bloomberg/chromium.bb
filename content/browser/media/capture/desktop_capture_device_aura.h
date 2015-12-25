// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_AURA_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_AURA_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/content/screen_capture_device_core.h"
#include "media/capture/video/video_capture_device.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {

// An implementation of VideoCaptureDevice that mirrors an Aura window.
class CONTENT_EXPORT DesktopCaptureDeviceAura
    : public media::VideoCaptureDevice {
 public:
  // Creates a VideoCaptureDevice for the Aura desktop.  If |source| does not
  // reference a registered aura window, returns nullptr instead.
  static scoped_ptr<media::VideoCaptureDevice> Create(
      const DesktopMediaID& source);

  ~DesktopCaptureDeviceAura() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        scoped_ptr<Client> client) override;
  void StopAndDeAllocate() override;

 private:
  explicit DesktopCaptureDeviceAura(const DesktopMediaID& source);

  scoped_ptr<media::ScreenCaptureDeviceCore> core_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureDeviceAura);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_AURA_H_
