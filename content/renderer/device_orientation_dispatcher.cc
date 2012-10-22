// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_orientation_dispatcher.h"

#include "content/common/device_orientation_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationController.h"

namespace content {

DeviceOrientationDispatcher::DeviceOrientationDispatcher(
    RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      controller_(NULL),
      started_(false) {
}

DeviceOrientationDispatcher::~DeviceOrientationDispatcher() {
  if (started_)
    stopUpdating();
}

bool DeviceOrientationDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationDispatcher, msg)
    IPC_MESSAGE_HANDLER(DeviceOrientationMsg_Updated,
                        OnDeviceOrientationUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationDispatcher::setController(
    WebKit::WebDeviceOrientationController* controller) {
  controller_.reset(controller);
}

void DeviceOrientationDispatcher::startUpdating() {
  Send(new DeviceOrientationHostMsg_StartUpdating(routing_id()));
  started_ = true;
}

void DeviceOrientationDispatcher::stopUpdating() {
  Send(new DeviceOrientationHostMsg_StopUpdating(routing_id()));
  started_ = false;
}

WebKit::WebDeviceOrientation DeviceOrientationDispatcher::lastOrientation()
    const {
   return last_orientation_;
}

namespace {
bool OrientationsEqual(const DeviceOrientationMsg_Updated_Params& a,
                       WebKit::WebDeviceOrientation* b) {
  if (a.can_provide_alpha != b->canProvideAlpha())
    return false;
  if (a.can_provide_alpha && a.alpha != b->alpha())
    return false;
  if (a.can_provide_beta != b->canProvideBeta())
    return false;
  if (a.can_provide_beta && a.beta != b->beta())
    return false;
  if (a.can_provide_gamma != b->canProvideGamma())
    return false;
  if (a.can_provide_gamma && a.gamma != b->gamma())
    return false;
  if (a.can_provide_absolute != b->canProvideAbsolute())
    return false;
  if (a.can_provide_absolute && a.absolute != b->absolute())
    return false;

  return true;
}
}  // namespace

void DeviceOrientationDispatcher::OnDeviceOrientationUpdated(
    const DeviceOrientationMsg_Updated_Params& p) {
  if (!last_orientation_.isNull() && OrientationsEqual(p, &last_orientation_))
    return;

  last_orientation_.setNull(false);
  if (p.can_provide_alpha)
    last_orientation_.setAlpha(p.alpha);
  if (p.can_provide_beta)
    last_orientation_.setBeta(p.beta);
  if (p.can_provide_gamma)
    last_orientation_.setGamma(p.gamma);
  if (p.can_provide_absolute)
    last_orientation_.setAbsolute(p.absolute);
  controller_->didChangeDeviceOrientation(last_orientation_);
}

}  // namespace content
