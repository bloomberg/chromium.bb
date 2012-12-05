// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_EVENT_HANDLER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_EVENT_HANDLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/pepper/pepper_plugin_delegate_impl.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace content {

class PepperDeviceEnumerationEventHandler
    : public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<PepperDeviceEnumerationEventHandler> {
 public:
  PepperDeviceEnumerationEventHandler();
  virtual ~PepperDeviceEnumerationEventHandler();

  int RegisterEnumerateDevicesCallback(
      const webkit::ppapi::PluginDelegate::EnumerateDevicesCallback& callback);
  void UnregisterEnumerateDevicesCallback(int request_id);

  int RegisterOpenDeviceCallback(
      const PepperPluginDelegateImpl::OpenDeviceCallback& callback);

  // MediaStreamDispatcherEventHandler implementation.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_device_array,
      const StreamDeviceInfoArray& video_device_array) OVERRIDE;
  virtual void OnStreamGenerationFailed(int request_id) OVERRIDE;
  virtual void OnDevicesEnumerated(
      int request_id,
      const StreamDeviceInfoArray& device_array) OVERRIDE;
  virtual void OnDevicesEnumerationFailed(int request_id) OVERRIDE;
  virtual void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDeviceOpenFailed(int request_id) OVERRIDE;

  // Stream type conversion.
  static MediaStreamType FromPepperDeviceType(PP_DeviceType_Dev type);
  static PP_DeviceType_Dev FromMediaStreamType(MediaStreamType type);

 private:
  void NotifyDevicesEnumerated(
      int request_id,
      bool succeeded,
      const StreamDeviceInfoArray& device_array);

  void NotifyDeviceOpened(int request_id,
                          bool succeeded,
                          const std::string& label);

  int next_id_;

  typedef std::map<int,
                   webkit::ppapi::PluginDelegate::EnumerateDevicesCallback>
      EnumerateCallbackMap;
  EnumerateCallbackMap enumerate_callbacks_;

  typedef std::map<int, PepperPluginDelegateImpl::OpenDeviceCallback>
      OpenCallbackMap;
  OpenCallbackMap open_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PepperDeviceEnumerationEventHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_EVENT_HANDLER_H_
