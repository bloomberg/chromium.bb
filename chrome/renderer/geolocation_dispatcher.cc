// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/geolocation_dispatcher.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationServiceBridge.h"

using WebKit::WebFrame;

GeolocationDispatcher::GeolocationDispatcher(RenderView* render_view)
    : render_view_(render_view) {
  render_view_->Send(new ViewHostMsg_Geolocation_RegisterDispatcher(
      render_view_->routing_id()));
}

GeolocationDispatcher::~GeolocationDispatcher() {
  for (IDMap<WebKit::WebGeolocationServiceBridge>::iterator it(&bridges_map_);
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->onWebGeolocationServiceDestroyed();
  }
  render_view_->Send(new ViewHostMsg_Geolocation_UnregisterDispatcher(
      render_view_->routing_id()));
}

bool GeolocationDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GeolocationDispatcher, message)
    IPC_MESSAGE_HANDLER(ViewMsg_Geolocation_PermissionSet,
                        OnGeolocationPermissionSet)
    IPC_MESSAGE_HANDLER(ViewMsg_Geolocation_PositionUpdated,
                        OnGeolocationPositionUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeolocationDispatcher::requestPermissionForFrame(
    int bridge_id, const WebKit::WebURL& url) {
  render_view_->Send(new ViewHostMsg_Geolocation_RequestPermission(
      render_view_->routing_id(), bridge_id, GURL(url)));
}

void GeolocationDispatcher::cancelPermissionRequestForFrame(
    int bridge_id, const WebKit::WebURL& url) {
  render_view_->Send(new ViewHostMsg_Geolocation_CancelPermissionRequest(
      render_view_->routing_id(), bridge_id, GURL(url)));
}

void GeolocationDispatcher::startUpdating(
    int bridge_id, const WebKit::WebURL& url, bool enableHighAccuracy) {
  render_view_->Send(new ViewHostMsg_Geolocation_StartUpdating(
      render_view_->routing_id(), bridge_id, GURL(url),
      enableHighAccuracy));
}

void GeolocationDispatcher::stopUpdating(int bridge_id) {
  render_view_->Send(new ViewHostMsg_Geolocation_StopUpdating(
      render_view_->routing_id(), bridge_id));
}

void GeolocationDispatcher::suspend(int bridge_id) {
  render_view_->Send(new ViewHostMsg_Geolocation_Suspend(
      render_view_->routing_id(), bridge_id));
}

void GeolocationDispatcher::resume(int bridge_id) {
  render_view_->Send(new ViewHostMsg_Geolocation_Resume(
      render_view_->routing_id(), bridge_id));
}

int GeolocationDispatcher::attachBridge(
    WebKit::WebGeolocationServiceBridge* bridge) {
  return bridges_map_.Add(bridge);
}

void GeolocationDispatcher::detachBridge(int bridge_id) {
  bridges_map_.Remove(bridge_id);
}

void GeolocationDispatcher::OnGeolocationPermissionSet(int bridge_id,
                                                       bool allowed) {
  WebKit::WebGeolocationServiceBridge* bridge = bridges_map_.Lookup(bridge_id);
  if (bridge) {
    bridge->setIsAllowed(allowed);
  }
}

void GeolocationDispatcher::OnGeolocationPositionUpdated(
    const Geoposition& geoposition) {
  DCHECK(geoposition.IsInitialized());
  for (IDMap<WebKit::WebGeolocationServiceBridge>::iterator it(&bridges_map_);
       !it.IsAtEnd(); it.Advance()) {
    if (geoposition.IsValidFix()) {
      it.GetCurrentValue()->setLastPosition(
          geoposition.latitude, geoposition.longitude,
          geoposition.is_valid_altitude(), geoposition.altitude,
          geoposition.accuracy,
          geoposition.is_valid_altitude_accuracy(),
          geoposition.altitude_accuracy,
          geoposition.is_valid_heading(), geoposition.heading,
          geoposition.is_valid_speed(), geoposition.speed,
          static_cast<int64>(geoposition.timestamp.ToDoubleT() * 1000));
    } else {
      it.GetCurrentValue()->setLastError(
          geoposition.error_code,
          WebKit::WebString::fromUTF8(geoposition.error_message));
    }
  }
}
