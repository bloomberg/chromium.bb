// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GEOLOCATION_DISPATCHER_H_
#define CHROME_RENDERER_GEOLOCATION_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "ipc/ipc_message.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/GeolocationServiceBridgeChromium.h"

class GURL;
class RenderView;
struct Geoposition;

// GeolocationDispatcher is a delegate for Geolocation messages used by
// WebKit.
// It's the complement of GeolocationDispatcherHost (owned by RenderViewHost).
class GeolocationDispatcher : public WebKit::WebGeolocationServiceInterface {
 public:
  explicit GeolocationDispatcher(RenderView* render_view);
  virtual ~GeolocationDispatcher();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in render thread.
  bool OnMessageReceived(const IPC::Message& msg);

  // WebKit::GeolocationServiceInterfaceChromium.
  void requestPermissionForFrame(int bridge_id, const WebKit::WebURL& url);
  // TODO(bulach): remove this method once we roll:
  // https://bugs.webkit.org/show_bug.cgi?id=36012.
  void startUpdating(int bridge_id, bool enableHighAccuracy);
  void startUpdating(
      int bridge_id, const WebKit::WebURL& url, bool enableHighAccuracy);
  void stopUpdating(int bridge_id);
  void suspend(int bridge_id);
  void resume(int bridge_id);
  int attachBridge(WebKit::WebGeolocationServiceBridge* geolocation_service);
  void dettachBridge(int bridge_id);

 private:
  // Permission for using geolocation has been set.
  void OnGeolocationPermissionSet(int bridge_id, bool is_allowed);

  // We have an updated geolocation position.
  void OnGeolocationPositionUpdated(const Geoposition& geoposition);

  // An error has happened when fetching a geolocation position.
  void OnGeolocationError(int code, const std::string& message);

  RenderView* render_view_;

  // The geolocation services attached to this dispatcher.
  IDMap<WebKit::WebGeolocationServiceBridge> bridges_map_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcher);
};

#endif  // CHROME_RENDERER_GEOLOCATION_DISPATCHER_H_
