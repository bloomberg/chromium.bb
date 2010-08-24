// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/device_orientation_dispatcher.h"

#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDeviceOrientation.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDeviceOrientationController.h"

DeviceOrientationDispatcher::DeviceOrientationDispatcher(
    RenderView* render_view)
    : render_view_(render_view),
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
    IPC_MESSAGE_HANDLER(ViewMsg_DeviceOrientationUpdated,
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
  render_view_->Send(new ViewHostMsg_DeviceOrientation_StartUpdating(
      render_view_->routing_id()));
  started_ = true;
}

void DeviceOrientationDispatcher::stopUpdating() {
  render_view_->Send(new ViewHostMsg_DeviceOrientation_StopUpdating(
      render_view_->routing_id()));
  started_ = false;
}

WebKit::WebDeviceOrientation DeviceOrientationDispatcher::lastOrientation()
    const {
  if (!last_update_.get())
    return WebKit::WebDeviceOrientation::nullOrientation();

  return WebKit::WebDeviceOrientation(last_update_->can_provide_alpha,
                                      last_update_->alpha,
                                      last_update_->can_provide_beta,
                                      last_update_->beta,
                                      last_update_->can_provide_gamma,
                                      last_update_->gamma);
}

void DeviceOrientationDispatcher::OnDeviceOrientationUpdated(
    const ViewMsg_DeviceOrientationUpdated_Params& p) {
  last_update_.reset(new ViewMsg_DeviceOrientationUpdated_Params(p));

  WebKit::WebDeviceOrientation orientation(p.can_provide_alpha, p.alpha,
                                           p.can_provide_beta, p.beta,
                                           p.can_provide_gamma, p.gamma);

  controller_->didChangeDeviceOrientation(orientation);
}
