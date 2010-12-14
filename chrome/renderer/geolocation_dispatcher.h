// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GEOLOCATION_DISPATCHER_H_
#define CHROME_RENDERER_GEOLOCATION_DISPATCHER_H_
#pragma once

#if defined(ENABLE_CLIENT_BASED_GEOLOCATION)

#include "base/scoped_ptr.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationController.h"

class RenderView;
struct Geoposition;

namespace WebKit {
class WebGeolocationController;
class WebGeolocationPermissionRequest;
class WebGeolocationPermissionRequestManager;
class WebGeolocationPosition;
class WebSecurityOrigin;
}

namespace IPC {
class Message;
}

// GeolocationDispatcher is a delegate for Geolocation messages used by
// WebKit.
// It's the complement of GeolocationDispatcherHost (owned by RenderViewHost).
class GeolocationDispatcher : public WebKit::WebGeolocationClient {
 public:
  explicit GeolocationDispatcher(RenderView* render_view);
  virtual ~GeolocationDispatcher();

  // IPC
  bool OnMessageReceived(const IPC::Message& message);

  // WebGeolocationClient
  virtual void geolocationDestroyed();
  virtual void startUpdating();
  virtual void stopUpdating();
  virtual void setEnableHighAccuracy(bool enable_high_accuracy);
  virtual void setController(WebKit::WebGeolocationController* controller);
  virtual bool lastPosition(WebKit::WebGeolocationPosition& position);
  virtual void requestPermission(
      const WebKit::WebGeolocationPermissionRequest& permissionRequest);
  virtual void cancelPermissionRequest(
      const WebKit::WebGeolocationPermissionRequest& permissionRequest);

 private:
  // Permission for using geolocation has been set.
  void OnGeolocationPermissionSet(int bridge_id, bool is_allowed);

  // We have an updated geolocation position or error code.
  void OnGeolocationPositionUpdated(const Geoposition& geoposition);

  // GeolocationDispatcher is owned by RenderView. Back pointer for IPC.
  RenderView* render_view_;

  // The controller_ is valid for the lifetime of the underlying
  // WebCore::GeolocationController. geolocationDestroyed() is
  // invoked when the underlying object is destroyed.
  scoped_ptr< WebKit::WebGeolocationController> controller_;

  scoped_ptr<WebKit::WebGeolocationPermissionRequestManager>
      pending_permissions_;
  bool enable_high_accuracy_;
  bool updating_;
};
#endif // ENABLE_CLIENT_BASED_GEOLOCATION

#endif // CHROME_RENDERER_GEOLOCATION_DISPATCHER_H_
