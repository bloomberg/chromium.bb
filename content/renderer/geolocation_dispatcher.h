// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
#define CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationController.h"

class RenderViewImpl;
struct Geoposition;

namespace WebKit {
class WebGeolocationController;
class WebGeolocationPermissionRequest;
class WebGeolocationPermissionRequestManager;
class WebGeolocationPosition;
}

// GeolocationDispatcher is a delegate for Geolocation messages used by
// WebKit.
// It's the complement of GeolocationDispatcherHost (owned by RenderViewHost).
class GeolocationDispatcher : public content::RenderViewObserver,
                              public WebKit::WebGeolocationClient {
 public:
  explicit GeolocationDispatcher(RenderViewImpl* render_view);
  virtual ~GeolocationDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

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

  // Permission for using geolocation has been set.
  void OnPermissionSet(int bridge_id, bool is_allowed);

  // We have an updated geolocation position or error code.
  void OnPositionUpdated(const Geoposition& geoposition);

  // The controller_ is valid for the lifetime of the underlying
  // WebCore::GeolocationController. geolocationDestroyed() is
  // invoked when the underlying object is destroyed.
  scoped_ptr< WebKit::WebGeolocationController> controller_;

  scoped_ptr<WebKit::WebGeolocationPermissionRequestManager>
      pending_permissions_;
  bool enable_high_accuracy_;
  bool updating_;
};

#endif  // CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
