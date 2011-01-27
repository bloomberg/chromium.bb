// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
#define CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClient.h"

#include "base/scoped_ptr.h"
#include "chrome/renderer/render_view_observer.h"

namespace WebKit { class WebDeviceOrientation; }

struct ViewMsg_DeviceOrientationUpdated_Params;

class DeviceOrientationDispatcher : public RenderViewObserver,
                                    public WebKit::WebDeviceOrientationClient {
 public:
  explicit DeviceOrientationDispatcher(RenderView* render_view);
  virtual ~DeviceOrientationDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // From WebKit::WebDeviceOrientationClient.
  virtual void setController(
      WebKit::WebDeviceOrientationController* controller);
  virtual void startUpdating();
  virtual void stopUpdating();
  virtual WebKit::WebDeviceOrientation lastOrientation() const;

  void OnDeviceOrientationUpdated(
      const ViewMsg_DeviceOrientationUpdated_Params& p);

  scoped_ptr<WebKit::WebDeviceOrientationController> controller_;
  scoped_ptr<WebKit::WebDeviceOrientation> last_orientation_;
  bool started_;
};

#endif  // CHROME_RENDERER_DEVICE_ORIENTATION_DISPATCHER_H_
