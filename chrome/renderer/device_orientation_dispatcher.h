// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
#define CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_

#include "third_party/WebKit/WebKit/chromium/public/WebDeviceOrientationClient.h"

#include "chrome/common/render_messages.h"

class RenderView;
namespace IPC { class Message; }

class DeviceOrientationDispatcher : public WebKit::WebDeviceOrientationClient {
 public:
  explicit DeviceOrientationDispatcher(RenderView* render_view);
  virtual ~DeviceOrientationDispatcher();

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
  scoped_ptr<ViewMsg_DeviceOrientationUpdated_Params> last_update_;
  bool started_;
};

#endif  // CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
