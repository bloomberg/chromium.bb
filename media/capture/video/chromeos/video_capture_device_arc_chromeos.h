// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_ARC_CHROMEOS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_ARC_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/power_manager_client.h"
#include "media/capture/video/chromeos/display_rotation_observer.h"
#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

namespace display {

class Display;

}  // namespace display

namespace media {

class CameraHalDelegate;
class CameraDeviceContext;
class CameraDeviceDelegate;

// Implementation of VideoCaptureDevice for ChromeOS with ARC++ camera HALv3.
class CAPTURE_EXPORT VideoCaptureDeviceArcChromeOS final
    : public VideoCaptureDevice,
      public DisplayRotationObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  VideoCaptureDeviceArcChromeOS(
      scoped_refptr<base::SingleThreadTaskRunner>
          task_runner_for_screen_observer,
      const VideoCaptureDeviceDescriptor& device_descriptor,
      scoped_refptr<CameraHalDelegate> camera_hal_delegate);

  ~VideoCaptureDeviceArcChromeOS() final;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void StopAndDeAllocate() final;
  void TakePhoto(TakePhotoCallback callback) final;
  void GetPhotoState(GetPhotoStateCallback callback) final;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) final;

  // chromeos::PowerManagerClient::Observer callbacks for system suspend and
  // resume events.
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) final;
  void SuspendDone(const base::TimeDelta& sleep_duration) final;

 private:
  void OpenDevice();
  void CloseDevice(base::Closure callback);

  // DisplayRotationDelegate implementation.
  void SetDisplayRotation(const display::Display& display) final;
  void SetRotation(int rotation);

  const VideoCaptureDeviceDescriptor device_descriptor_;

  // A reference to the CameraHalDelegate instance in the VCD factory.  This is
  // used by AllocateAndStart to query camera info and create the camera device.
  const scoped_refptr<CameraHalDelegate> camera_hal_delegate_;

  // A reference to the thread that all the VideoCaptureDevice interface methods
  // are expected to be called on.
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  // The thread that all the Mojo operations of |camera_device_delegate_| take
  // place.  Started in AllocateAndStart and stopped in StopAndDeAllocate, where
  // the access to the base::Thread methods are sequenced on
  // |capture_task_runner_|.
  base::Thread camera_device_ipc_thread_;

  VideoCaptureParams capture_params_;
  // |device_context_| is created and owned by VideoCaptureDeviceArcChromeOS
  // and is only accessed by |camera_device_delegate_|.
  std::unique_ptr<CameraDeviceContext> device_context_;

  // Internal delegate doing the actual capture setting, buffer allocation and
  // circulation with the camera HAL. Created in AllocateAndStart and deleted in
  // StopAndDeAllocate on |capture_task_runner_|.  All methods of
  // |camera_device_delegate_| operate on |camera_device_ipc_thread_|.
  std::unique_ptr<CameraDeviceDelegate> camera_device_delegate_;

  scoped_refptr<ScreenObserverDelegate> screen_observer_delegate_;
  const VideoFacingMode lens_facing_;
  const int camera_orientation_;
  // Whether the incoming frames should rotate when the device rotates.
  const bool rotates_with_device_;
  int rotation_;

  base::WeakPtrFactory<VideoCaptureDeviceArcChromeOS> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceArcChromeOS);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_ARC_CHROMEOS_H_
