// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

// CameraHalDelegate is the component which does Mojo IPCs to the camera HAL
// process on Chrome OS to access the module-level camera functionalities such
// as camera device info look-up and opening camera devices.
//
// CameraHalDelegate is refcounted because VideoCaptureDeviceFactoryChromeOS and
// CameraDeviceDelegate both need to reference CameraHalDelegate, and
// VideoCaptureDeviceFactoryChromeOS may be destroyed while CameraDeviceDelegate
// is still alive.
class CAPTURE_EXPORT CameraHalDelegate final
    : public base::RefCountedThreadSafe<CameraHalDelegate>,
      public arc::mojom::CameraModuleCallbacks {
 public:
  // All the Mojo IPC operations happen on |ipc_task_runner|.
  explicit CameraHalDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  // Establishes the Mojo IPC channel to the camera HAL adapter.  This method
  // should be called before any other methods of CameraHalDelegate is called.
  bool StartCameraModuleIpc();

  // Resets |camera_module_| and |camera_module_callbacks_|.
  void Reset();

  // Delegation methods for the VideoCaptureDeviceFactory interface.  These
  // methods are called by VideoCaptureDeviceFactoryChromeOS directly.  They
  // operate on the same thread that the VideoCaptureDeviceFactoryChromeOS runs
  // on.
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      scoped_refptr<base::SingleThreadTaskRunner>
          task_runner_for_screen_observer,
      const VideoCaptureDeviceDescriptor& device_descriptor);
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats);
  void GetDeviceDescriptors(VideoCaptureDeviceDescriptors* device_descriptors);

  // Asynchronous method to get the camera info of |camera_id|.  This method may
  // be called on any thread.
  using GetCameraInfoCallback =
      base::Callback<void(int32_t, arc::mojom::CameraInfoPtr)>;
  void GetCameraInfo(int32_t camera_id, const GetCameraInfoCallback& callback);

  // Asynchronous method to open the camera device designated by |camera_id|.
  // This method may be called on any thread; |callback| will run on
  // |ipc_task_runner_|.
  using OpenDeviceCallback = base::Callback<void(int32_t)>;
  void OpenDevice(int32_t camera_id,
                  arc::mojom::Camera3DeviceOpsRequest device_ops_request,
                  const OpenDeviceCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<CameraHalDelegate>;

  ~CameraHalDelegate() final;

  friend class CameraHalDelegateTest;
  friend class CameraDeviceDelegateTest;
  void StartForTesting(arc::mojom::CameraModulePtrInfo info);

  // Resets the Mojo interface and bindings.
  void ResetMojoInterfaceOnIpcThread();

  // Internal method to update the camera info for all built-in cameras. Runs on
  // the same thread as CreateDevice, GetSupportedFormats, and
  // GetDeviceDescriptors.
  bool UpdateBuiltInCameraInfo();
  void UpdateBuiltInCameraInfoOnIpcThread();
  // Callback for GetNumberOfCameras Mojo IPC function.  GetNumberOfCameras
  // returns the number of built-in cameras on the device.
  void OnGotNumberOfCamerasOnIpcThread(int32_t num_cameras);
  // Callback for SetCallbacks Mojo IPC function. SetCallbacks is called after
  // GetNumberOfCameras is called for the first time, and before any other calls
  // to |camera_module_|.
  void OnSetCallbacksOnIpcThread(int32_t result);
  void GetCameraInfoOnIpcThread(int32_t camera_id,
                                const GetCameraInfoCallback& callback);
  void OnGotCameraInfoOnIpcThread(int32_t camera_id,
                                  int32_t result,
                                  arc::mojom::CameraInfoPtr camera_info);

  // Called by OpenDevice to actually open the device specified by |camera_id|.
  // This method runs on |ipc_task_runner_|.
  void OpenDeviceOnIpcThread(
      int32_t camera_id,
      arc::mojom::Camera3DeviceOpsRequest device_ops_request,
      const OpenDeviceCallback& callback);

  // CameraModuleCallbacks implementation. Operates on |ipc_task_runner_|.
  void CameraDeviceStatusChange(
      int32_t camera_id,
      arc::mojom::CameraDeviceStatus new_status) final;

  // Signaled when |num_builtin_cameras_| and |camera_info_| are updated.
  // Queried and waited by UpdateBuiltInCameraInfo, signaled by
  // OnGotCameraInfoOnIpcThread.
  base::WaitableEvent builtin_camera_info_updated_;

  // |num_builtin_cameras_| stores the number of built-in camera devices
  // reported by the camera HAL, and |camera_info_| stores the camera info of
  // each camera device. They are modified only on |ipc_task_runner_|. They
  // are also read in GetSupportedFormats and GetDeviceDescriptors, in which the
  // access is sequenced through UpdateBuiltInCameraInfo and
  // |builtin_camera_info_updated_| to avoid race conditions.
  size_t num_builtin_cameras_;
  std::unordered_map<std::string, arc::mojom::CameraInfoPtr> camera_info_;

  SEQUENCE_CHECKER(sequence_checker_);

  // The task runner where all the camera module Mojo communication takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // The Mojo proxy to access the camera module at the remote camera HAL.  Bound
  // to |ipc_task_runner_|.
  arc::mojom::CameraModulePtr camera_module_;

  // The Mojo binding serving the camera module callbacks.  Bound to
  // |ipc_task_runner_|.
  mojo::Binding<arc::mojom::CameraModuleCallbacks> camera_module_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CameraHalDelegate);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_
