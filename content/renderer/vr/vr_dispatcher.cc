// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/vr/vr_dispatcher.h"

#include <stddef.h>

#include "content/public/common/service_registry.h"
#include "content/renderer/vr/vr_type_converters.h"

namespace content {

VRDispatcher::VRDispatcher(ServiceRegistry* service_registry)
    : service_registry_(service_registry) {
}

VRDispatcher::~VRDispatcher() {
}

VRServicePtr& VRDispatcher::GetVRServicePtr() {
  if (!vr_service_) {
    service_registry_->ConnectToRemoteService(mojo::GetProxy(&vr_service_));
  }
  return vr_service_;
}

void VRDispatcher::getDevices(blink::WebVRGetDevicesCallback* callback) {
  int request_id = pending_requests_.Add(callback);
  GetVRServicePtr()->GetDevices(base::Bind(&VRDispatcher::OnGetDevices,
                                           base::Unretained(this), request_id));
}

void VRDispatcher::getSensorState(unsigned int index,
                                  blink::WebHMDSensorState& state) {
  GetVRServicePtr()->GetSensorState(
      index,
      base::Bind(&VRDispatcher::OnGetSensorState, base::Unretained(&state)));

  // This call needs to return results synchronously in order to be useful and
  // provide the lowest latency results possible.
  GetVRServicePtr().WaitForIncomingResponse();
}

void VRDispatcher::resetSensor(unsigned int index) {
  GetVRServicePtr()->ResetSensor(index);
}

void VRDispatcher::OnGetDevices(int request_id,
                                const mojo::Array<VRDeviceInfoPtr>& devices) {
  blink::WebVector<blink::WebVRDevice> web_devices(devices.size());

  blink::WebVRGetDevicesCallback* callback =
      pending_requests_.Lookup(request_id);
  if (!callback)
    return;

  for (size_t i = 0; i < devices.size(); ++i) {
    web_devices[i] = devices[i].To<blink::WebVRDevice>();
  }

  callback->onSuccess(web_devices);
  pending_requests_.Remove(request_id);
}

void VRDispatcher::OnGetSensorState(blink::WebHMDSensorState* state,
                                    const VRSensorStatePtr& mojo_state) {
  *state = mojo_state.To<blink::WebHMDSensorState>();
}

}  // namespace content
