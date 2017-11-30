// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/media/media_devices.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "ppapi/c/pp_instance.h"
#include "third_party/WebKit/public/platform/modules/mediastream/media_devices.mojom.h"

namespace content {
class MediaStreamDeviceObserver;

class PepperMediaDeviceManager
    : public PepperDeviceEnumerationHostHelper::Delegate,
      public RenderFrameObserver,
      public RenderFrameObserverTracker<PepperMediaDeviceManager>,
      public base::SupportsWeakPtr<PepperMediaDeviceManager> {
 public:
  static base::WeakPtr<PepperMediaDeviceManager> GetForRenderFrame(
      RenderFrame* render_frame);
  ~PepperMediaDeviceManager() override;

  // PepperDeviceEnumerationHostHelper::Delegate implementation:
  void EnumerateDevices(PP_DeviceType_Dev type,
                        const DevicesCallback& callback) override;
  uint32_t StartMonitoringDevices(PP_DeviceType_Dev type,
                                  const DevicesCallback& callback) override;
  void StopMonitoringDevices(PP_DeviceType_Dev type,
                             uint32_t subscription_id) override;

  typedef base::Callback<void(int /* request_id */,
                              bool /* succeeded */,
                              const std::string& /* label */)>
      OpenDeviceCallback;

  // Opens the specified device. The request ID passed into the callback will be
  // the same as the return value. If successful, the label passed into the
  // callback identifies a audio/video steam, which can be used to call
  // CloseDevice() and GetSesssionID().
  int OpenDevice(PP_DeviceType_Dev type,
                 const std::string& device_id,
                 PP_Instance pp_instance,
                 const OpenDeviceCallback& callback);
  // Cancels an request to open device, using the request ID returned by
  // OpenDevice(). It is guaranteed that the callback passed into OpenDevice()
  // won't be called afterwards.
  void CancelOpenDevice(int request_id);
  void CloseDevice(const std::string& label);
  // Gets audio/video session ID given a label.
  int GetSessionID(PP_DeviceType_Dev type, const std::string& label);

  // Stream type conversion.
  static MediaStreamType FromPepperDeviceType(PP_DeviceType_Dev type);

 private:
  explicit PepperMediaDeviceManager(RenderFrame* render_frame);

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Called by StopEnumerateDevices() after returing to the event loop, to avoid
  // a reentrancy problem.
  void StopEnumerateDevicesDelayed(int request_id);

  void OnDeviceOpened(int request_id,
                      bool success,
                      const std::string& label,
                      const MediaStreamDevice& device);

  void DevicesEnumerated(const DevicesCallback& callback,
                         MediaDeviceType type,
                         const std::vector<MediaDeviceInfoArray>& enumeration);

  void DevicesChanged(const DevicesCallback& callback,
                      MediaDeviceType type,
                      const MediaDeviceInfoArray& enumeration);

  const mojom::MediaStreamDispatcherHostPtr& GetMediaStreamDispatcherHost();
  MediaStreamDeviceObserver* GetMediaStreamDeviceObserver() const;
  const blink::mojom::MediaDevicesDispatcherHostPtr&
  GetMediaDevicesDispatcher();

  int next_id_;

  typedef std::map<int, OpenDeviceCallback> OpenCallbackMap;
  OpenCallbackMap open_callbacks_;

  mojom::MediaStreamDispatcherHostPtr dispatcher_host_;
  blink::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaDeviceManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_
