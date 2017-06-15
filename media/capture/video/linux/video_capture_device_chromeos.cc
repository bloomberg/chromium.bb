// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_chromeos.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace media {

static CameraConfigChromeOS* GetCameraConfig() {
  static CameraConfigChromeOS* config = new CameraConfigChromeOS();
  return config;
}

// This is a delegate class used to transfer Display change events from the UI
// thread to the media thread.
class VideoCaptureDeviceChromeOS::ScreenObserverDelegate
    : public display::DisplayObserver,
      public base::RefCountedThreadSafe<ScreenObserverDelegate> {
 public:
  ScreenObserverDelegate(
      VideoCaptureDeviceChromeOS* capture_device,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : capture_device_(capture_device),
        ui_task_runner_(ui_task_runner),
        capture_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ScreenObserverDelegate::AddObserverOnUIThread, this));
  }

  void RemoveObserver() {
    DCHECK(capture_task_runner_->BelongsToCurrentThread());
    capture_device_ = NULL;
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ScreenObserverDelegate::RemoveObserverOnUIThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<ScreenObserverDelegate>;

  ~ScreenObserverDelegate() override { DCHECK(!capture_device_); }

  void OnDisplayAdded(const display::Display& /*new_display*/) override {}
  void OnDisplayRemoved(const display::Display& /*old_display*/) override {}
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    if (!(metrics & DISPLAY_METRIC_ROTATION))
      return;
    SendDisplayRotation(display);
  }

  void AddObserverOnUIThread() {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    display::Screen* screen = display::Screen::GetScreen();
    if (screen) {
      screen->AddObserver(this);
      SendDisplayRotation(screen->GetPrimaryDisplay());
    }
  }

  void RemoveObserverOnUIThread() {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    display::Screen* screen = display::Screen::GetScreen();
    if (screen)
      screen->RemoveObserver(this);
  }

  // Post the screen rotation change from the UI thread to capture thread
  void SendDisplayRotation(const display::Display& display) {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    capture_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ScreenObserverDelegate::SendDisplayRotationOnCaptureThread,
                   this, display));
  }

  void SendDisplayRotationOnCaptureThread(const display::Display& display) {
    DCHECK(capture_task_runner_->BelongsToCurrentThread());
    if (capture_device_)
      capture_device_->SetDisplayRotation(display);
  }

  VideoCaptureDeviceChromeOS* capture_device_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScreenObserverDelegate);
};

VideoCaptureDeviceChromeOS::VideoCaptureDeviceChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : VideoCaptureDeviceLinux(device_descriptor),
      screen_observer_delegate_(
          new ScreenObserverDelegate(this, ui_task_runner)),
      lens_facing_(
          GetCameraConfig()->GetCameraFacing(device_descriptor.device_id,
                                             device_descriptor.model_id)),
      camera_orientation_(
          GetCameraConfig()->GetOrientation(device_descriptor.device_id,
                                            device_descriptor.model_id)),
      // External cameras have lens_facing as MEDIA_VIDEO_FACING_NONE.
      // We don't want to rotate the frame even if the device rotates.
      rotates_with_device_(lens_facing_ !=
                           VideoFacingMode::MEDIA_VIDEO_FACING_NONE) {}

VideoCaptureDeviceChromeOS::~VideoCaptureDeviceChromeOS() {
  screen_observer_delegate_->RemoveObserver();
}

void VideoCaptureDeviceChromeOS::SetRotation(int rotation) {
  if (!rotates_with_device_) {
    rotation = 0;
  } else if (lens_facing_ == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT) {
    // Original frame when |rotation| = 0
    // -----------------------
    // |          *          |
    // |         * *         |
    // |        *   *        |
    // |       *******       |
    // |      *       *      |
    // |     *         *     |
    // -----------------------
    //
    // |rotation| = 90, this is what back camera sees
    // -----------------------
    // |    ********         |
    // |       *   ****      |
    // |       *      ***    |
    // |       *      ***    |
    // |       *   ****      |
    // |    ********         |
    // -----------------------
    //
    // |rotation| = 90, this is what front camera sees
    // -----------------------
    // |         ********    |
    // |      ****   *       |
    // |    ***      *       |
    // |    ***      *       |
    // |      ****   *       |
    // |         ********    |
    // -----------------------
    //
    // Therefore, for back camera, we need to rotate (360 - |rotation|).
    rotation = (360 - rotation) % 360;
  }
  // Take into account camera orientation w.r.t. the display. External cameras
  // would have camera_orientation_ as 0.
  rotation = (rotation + camera_orientation_) % 360;
  VideoCaptureDeviceLinux::SetRotation(rotation);
}

void VideoCaptureDeviceChromeOS::SetDisplayRotation(
    const display::Display& display) {
  if (display.IsInternal())
    SetRotation(display.rotation() * 90);
}

}  // namespace media
