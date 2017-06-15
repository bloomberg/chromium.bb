// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

class CameraHalDelegate;
class CameraDeviceContext;
class StreamBufferManager;

// The interface to register buffer with and send capture request to the
// camera HAL.
class CAPTURE_EXPORT StreamCaptureInterface {
 public:
  struct Plane {
    Plane();
    ~Plane();
    mojo::ScopedHandle fd;
    uint32_t stride;
    uint32_t offset;
  };

  virtual ~StreamCaptureInterface() {}

  // Registers a buffer to the camera HAL.
  virtual void RegisterBuffer(uint64_t buffer_id,
                              arc::mojom::Camera3DeviceOps::BufferType type,
                              uint32_t drm_format,
                              arc::mojom::HalPixelFormat hal_pixel_format,
                              uint32_t width,
                              uint32_t height,
                              std::vector<Plane> planes,
                              base::OnceCallback<void(int32_t)> callback);

  // Sends a capture request to the camera HAL.
  virtual void ProcessCaptureRequest(
      arc::mojom::Camera3CaptureRequestPtr request,
      base::OnceCallback<void(int32_t)> callback);
};

// CameraDeviceDelegate is instantiated on the capture thread where
// AllocateAndStart of VideoCaptureDeviceArcChromeOS runs on.  All the methods
// in CameraDeviceDelegate run on |ipc_task_runner_| and hence all the
// access to member variables is sequenced.
class CAPTURE_EXPORT CameraDeviceDelegate final {
 public:
  CameraDeviceDelegate(
      VideoCaptureDeviceDescriptor device_descriptor,
      scoped_refptr<CameraHalDelegate> camera_hal_delegate,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  ~CameraDeviceDelegate();

  // Delegation methods for the VideoCaptureDevice interface.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<VideoCaptureDevice::Client> client);
  void StopAndDeAllocate(base::Closure device_close_callback);
  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);
  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);

  // Sets the frame rotation angle in |rotation_|.  |rotation_| is clockwise
  // rotation in degrees, and is passed to |client_| along with the captured
  // frames.
  void SetRotation(int rotation);

  base::WeakPtr<CameraDeviceDelegate> GetWeakPtr();

 private:
  class StreamCaptureInterfaceImpl;

  friend class CameraDeviceDelegateTest;

  // Mojo connection error handler.
  void OnMojoConnectionError();

  // Callback method for the Close Mojo IPC call.  This method resets the Mojo
  // connection and closes the camera device.
  void OnClosed(int32_t result);

  // Resets the Mojo interface and bindings.
  void ResetMojoInterface();

  // Sets |static_metadata_| from |camera_info|.
  void OnGotCameraInfo(int32_t result, arc::mojom::CameraInfoPtr camera_info);

  // Creates the Mojo connection to the camera device.
  void OnOpenedDevice(int32_t result);

  // Initializes the camera HAL.  Initialize sets up the Camera3CallbackOps with
  // the camera HAL.  OnInitialized continues to ConfigureStreams if the
  // Initialize call succeeds.
  void Initialize();
  void OnInitialized(int32_t result);

  // ConfigureStreams sets up stream context in |streams_| and configure the
  // streams with the camera HAL.  OnConfiguredStreams updates |streams_| with
  // the stream config returned, and allocates buffers as per |updated_config|
  // indicates.  If there's no error OnConfiguredStreams notifies
  // |client_| the capture has started by calling OnStarted, and proceeds to
  // ConstructDefaultRequestSettings.
  void ConfigureStreams();
  void OnConfiguredStreams(
      int32_t result,
      arc::mojom::Camera3StreamConfigurationPtr updated_config);

  // ConstructDefaultRequestSettings asks the camera HAL for the default request
  // settings of the stream in |stream_context_|.
  // OnConstructedDefaultRequestSettings sets the request settings in
  // |streams_context_|.  If there's no error
  // OnConstructedDefaultRequestSettings calls StartCapture to start the video
  // capture loop.
  void ConstructDefaultRequestSettings();
  void OnConstructedDefaultRequestSettings(
      arc::mojom::CameraMetadataPtr settings);

  // StreamCaptureInterface implementations.  These methods are called by
  // |stream_buffer_manager_| on |ipc_task_runner_|.
  void RegisterBuffer(uint64_t buffer_id,
                      arc::mojom::Camera3DeviceOps::BufferType type,
                      uint32_t drm_format,
                      arc::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      std::vector<StreamCaptureInterface::Plane> planes,
                      base::OnceCallback<void(int32_t)> callback);
  void ProcessCaptureRequest(arc::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback);

  const VideoCaptureDeviceDescriptor device_descriptor_;

  int32_t camera_id_;

  const scoped_refptr<CameraHalDelegate> camera_hal_delegate_;

  VideoCaptureParams chrome_capture_params_;

  std::unique_ptr<CameraDeviceContext> device_context_;

  std::unique_ptr<StreamBufferManager> stream_buffer_manager_;

  // Stores the static camera characteristics of the camera device. E.g. the
  // supported formats and resolution, various available exposure and apeture
  // settings, etc.
  arc::mojom::CameraMetadataPtr static_metadata_;

  arc::mojom::Camera3DeviceOpsPtr device_ops_;

  // Where all the Mojo IPC calls takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  base::Closure device_close_callback_;

  base::WeakPtrFactory<CameraDeviceDelegate> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CameraDeviceDelegate);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_
