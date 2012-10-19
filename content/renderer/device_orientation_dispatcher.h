// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientation.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"

struct DeviceOrientationMsg_Updated_Params;

namespace content {
class RenderViewImpl;

class DeviceOrientationDispatcher : public RenderViewObserver,
                                    public WebKit::WebDeviceOrientationClient {
 public:
  explicit DeviceOrientationDispatcher(RenderViewImpl* render_view);
  virtual ~DeviceOrientationDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // From WebKit::WebDeviceOrientationClient.
  virtual void setController(
      WebKit::WebDeviceOrientationController* controller);
  virtual void startUpdating();
  virtual void stopUpdating();
  virtual WebKit::WebDeviceOrientation lastOrientation() const;

  void OnDeviceOrientationUpdated(
      const DeviceOrientationMsg_Updated_Params& p);

  scoped_ptr<WebKit::WebDeviceOrientationController> controller_;
  WebKit::WebDeviceOrientation last_orientation_;
  bool started_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
