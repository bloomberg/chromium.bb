// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_VR_DISPATCHER_H_
#define CONTENT_RENDERER_VR_DISPATCHER_H_

#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "content/common/vr_service.mojom.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/vr/WebVR.h"
#include "third_party/WebKit/public/platform/modules/vr/WebVRClient.h"

namespace content {

class ServiceRegistry;

class VRDispatcher : NON_EXPORTED_BASE(public blink::WebVRClient) {
 public:
  explicit VRDispatcher(ServiceRegistry* service_registry);
  ~VRDispatcher() override;

  // blink::WebVRClient implementation.
  void getDevices(blink::WebVRGetDevicesCallback* callback) override;

  void getSensorState(unsigned int index,
                      blink::WebHMDSensorState& state) override;

  void resetSensor(unsigned int index) override;

 private:
  // Helper method that returns an initialized PermissionServicePtr.
  VRServicePtr& GetVRServicePtr();

  // Callback handlers
  void OnGetDevices(int request_id,
                    const mojo::Array<VRDeviceInfoPtr>& devices);
  static void OnGetSensorState(blink::WebHMDSensorState* state,
                               const VRSensorStatePtr& mojo_state);

  // Tracks requests sent to browser to match replies with callbacks.
  // Owns callback objects.
  IDMap<blink::WebVRGetDevicesCallback, IDMapOwnPointer> pending_requests_;

  ServiceRegistry* service_registry_;
  VRServicePtr vr_service_;

  DISALLOW_COPY_AND_ASSIGN(VRDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_VR_DISPATCHER_H_
