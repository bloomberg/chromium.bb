// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClient.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"

class RenderViewImpl;

namespace WebKit {
class WebDeviceOrientation;
}

struct DeviceOrientationMsg_Updated_Params;

class DeviceOrientationDispatcher : public content::RenderViewObserver,
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
  scoped_ptr<WebKit::WebDeviceOrientation> last_orientation_;
  bool started_;
};

#endif  // CONTENT_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
