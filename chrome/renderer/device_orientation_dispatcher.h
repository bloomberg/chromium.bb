// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
#define CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_

#include "third_party/WebKit/WebKit/chromium/public/WebDeviceOrientationClient.h"

#include "base/scoped_ptr.h"
#include "ipc/ipc_channel.h"

class RenderView;
namespace WebKit { class WebDeviceOrientation; }

struct ViewMsg_DeviceOrientationUpdated_Params;

class DeviceOrientationDispatcher : public WebKit::WebDeviceOrientationClient,
                                    public IPC::Channel::Listener {
 public:
  explicit DeviceOrientationDispatcher(RenderView* render_view);
  virtual ~DeviceOrientationDispatcher();

  // IPC::Channel::Implementation.
  bool OnMessageReceived(const IPC::Message& msg);

  // From WebKit::WebDeviceOrientationClient.
  virtual void setController(
      WebKit::WebDeviceOrientationController* controller);
  virtual void startUpdating();
  virtual void stopUpdating();
  virtual WebKit::WebDeviceOrientation lastOrientation() const;

 private:
  void OnDeviceOrientationUpdated(
      const ViewMsg_DeviceOrientationUpdated_Params& p);

  RenderView* render_view_;
  scoped_ptr<WebKit::WebDeviceOrientationController> controller_;
  scoped_ptr<WebKit::WebDeviceOrientation> last_orientation_;
  bool started_;
};

#endif  // CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
