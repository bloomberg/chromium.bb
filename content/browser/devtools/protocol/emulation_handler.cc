// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/emulation_handler.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/common/url_constants.h"
#include "device/geolocation/public/cpp/geoposition.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace content {
namespace protocol {

namespace {

blink::WebScreenOrientationType WebScreenOrientationTypeFromString(
    const std::string& type) {
  if (type == Emulation::ScreenOrientation::TypeEnum::PortraitPrimary)
    return blink::kWebScreenOrientationPortraitPrimary;
  if (type == Emulation::ScreenOrientation::TypeEnum::PortraitSecondary)
    return blink::kWebScreenOrientationPortraitSecondary;
  if (type == Emulation::ScreenOrientation::TypeEnum::LandscapePrimary)
    return blink::kWebScreenOrientationLandscapePrimary;
  if (type == Emulation::ScreenOrientation::TypeEnum::LandscapeSecondary)
    return blink::kWebScreenOrientationLandscapeSecondary;
  return blink::kWebScreenOrientationUndefined;
}

ui::GestureProviderConfigType TouchEmulationConfigurationToType(
    const std::string& protocol_value) {
  ui::GestureProviderConfigType result =
      ui::GestureProviderConfigType::CURRENT_PLATFORM;
  if (protocol_value ==
      Emulation::SetEmitTouchEventsForMouse::ConfigurationEnum::Mobile) {
    result = ui::GestureProviderConfigType::GENERIC_MOBILE;
  }
  if (protocol_value ==
      Emulation::SetEmitTouchEventsForMouse::ConfigurationEnum::Desktop) {
    result = ui::GestureProviderConfigType::GENERIC_DESKTOP;
  }
  return result;
}

}  // namespace

EmulationHandler::EmulationHandler()
    : DevToolsDomainHandler(Emulation::Metainfo::domainName),
      touch_emulation_enabled_(false),
      device_emulation_enabled_(false),
      host_(nullptr) {
}

EmulationHandler::~EmulationHandler() {
}

void EmulationHandler::SetRenderer(int process_host_id,
                                   RenderFrameHostImpl* frame_host) {
  if (host_ == frame_host)
    return;

  host_ = frame_host;
  if (touch_emulation_enabled_)
    UpdateTouchEventEmulationState();
  UpdateDeviceEmulationState();
}

void EmulationHandler::Wire(UberDispatcher* dispatcher) {
  Emulation::Dispatcher::wire(dispatcher, this);
}

Response EmulationHandler::Disable() {
  if (touch_emulation_enabled_) {
    touch_emulation_enabled_ = false;
    UpdateTouchEventEmulationState();
  }
  device_emulation_enabled_ = false;
  UpdateDeviceEmulationState();
  return Response::OK();
}

Response EmulationHandler::SetGeolocationOverride(
    Maybe<double> latitude, Maybe<double> longitude, Maybe<double> accuracy) {
  if (!GetWebContents())
    return Response::InternalError();

  auto* geolocation_context = GetWebContents()->GetGeolocationContext();
  auto geoposition = device::mojom::Geoposition::New();
  if (latitude.isJust() && longitude.isJust() && accuracy.isJust()) {
    geoposition->latitude = latitude.fromJust();
    geoposition->longitude = longitude.fromJust();
    geoposition->accuracy = accuracy.fromJust();
    geoposition->timestamp = base::Time::Now();

    if (!device::ValidateGeoposition(*geoposition))
      return Response::Error("Invalid geolocation");

  } else {
    geoposition->error_code =
        device::mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  }
  geolocation_context->SetOverride(std::move(geoposition));
  return Response::OK();
}

Response EmulationHandler::ClearGeolocationOverride() {
  if (!GetWebContents())
    return Response::InternalError();

  auto* geolocation_context = GetWebContents()->GetGeolocationContext();
  geolocation_context->ClearOverride();
  return Response::OK();
}

Response EmulationHandler::SetEmitTouchEventsForMouse(
    bool enabled,
    Maybe<std::string> configuration) {
  touch_emulation_enabled_ = enabled;
  touch_emulation_configuration_ = configuration.fromMaybe("");
  UpdateTouchEventEmulationState();
  return Response::OK();
}

Response EmulationHandler::CanEmulate(bool* result) {
#if defined(OS_ANDROID)
  *result = false;
#else
  *result = true;
  if (WebContentsImpl* web_contents = GetWebContents())
    *result &= !web_contents->GetVisibleURL().SchemeIs(kChromeDevToolsScheme);
  if (host_ && host_->GetRenderWidgetHost())
    *result &= !host_->GetRenderWidgetHost()->auto_resize_enabled();
#endif  // defined(OS_ANDROID)
  return Response::OK();
}

Response EmulationHandler::SetDeviceMetricsOverride(
    int width,
    int height,
    double device_scale_factor,
    bool mobile,
    Maybe<double> scale,
    Maybe<int> screen_width,
    Maybe<int> screen_height,
    Maybe<int> position_x,
    Maybe<int> position_y,
    Maybe<bool> dont_set_visible_size,
    Maybe<Emulation::ScreenOrientation> screen_orientation,
    Maybe<protocol::Page::Viewport> viewport) {
  const static int max_size = 10000000;
  const static double max_scale = 10;
  const static int max_orientation_angle = 360;

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return Response::Error("Target does not support metrics override");

  if (screen_width.fromMaybe(0) < 0 || screen_height.fromMaybe(0) < 0 ||
      screen_width.fromMaybe(0) > max_size ||
      screen_height.fromMaybe(0) > max_size) {
    return Response::InvalidParams(
        "Screen width and height values must be positive, not greater than " +
        base::IntToString(max_size));
  }

  if (position_x.fromMaybe(0) < 0 || position_y.fromMaybe(0) < 0 ||
      position_x.fromMaybe(0) > screen_width.fromMaybe(0) ||
      position_y.fromMaybe(0) > screen_height.fromMaybe(0)) {
    return Response::InvalidParams("View position should be on the screen");
  }

  if (width < 0 || height < 0 || width > max_size || height > max_size) {
    return Response::InvalidParams(
        "Width and height values must be positive, not greater than " +
        base::IntToString(max_size));
  }

  if (device_scale_factor < 0)
    return Response::InvalidParams("deviceScaleFactor must be non-negative");

  if (scale.fromMaybe(1) <= 0 || scale.fromMaybe(1) > max_scale) {
    return Response::InvalidParams("scale must be positive, not greater than " +
                                   base::NumberToString(max_scale));
  }

  blink::WebScreenOrientationType orientationType =
      blink::kWebScreenOrientationUndefined;
  int orientationAngle = 0;
  if (screen_orientation.isJust()) {
    Emulation::ScreenOrientation* orientation = screen_orientation.fromJust();
    orientationType = WebScreenOrientationTypeFromString(
        orientation->GetType());
    if (orientationType == blink::kWebScreenOrientationUndefined)
      return Response::InvalidParams("Invalid screen orientation type value");
    orientationAngle = orientation->GetAngle();
    if (orientationAngle < 0 || orientationAngle >= max_orientation_angle) {
      return Response::InvalidParams(
          "Screen orientation angle must be non-negative, less than " +
          base::IntToString(max_orientation_angle));
    }
  }

  blink::WebDeviceEmulationParams params;
  params.screen_position = mobile ? blink::WebDeviceEmulationParams::kMobile
                                  : blink::WebDeviceEmulationParams::kDesktop;
  params.screen_size =
      blink::WebSize(screen_width.fromMaybe(0), screen_height.fromMaybe(0));
  if (position_x.isJust() && position_y.isJust()) {
    params.view_position =
        blink::WebPoint(position_x.fromMaybe(0), position_y.fromMaybe(0));
  }
  params.device_scale_factor = device_scale_factor;
  params.view_size = blink::WebSize(width, height);
  params.scale = scale.fromMaybe(1);
  params.screen_orientation_type = orientationType;
  params.screen_orientation_angle = orientationAngle;

  if (viewport.isJust()) {
    params.viewport_offset.x = viewport.fromJust()->GetX();
    params.viewport_offset.y = viewport.fromJust()->GetY();

    ScreenInfo screen_info;
    widget_host->GetScreenInfo(&screen_info);
    double dpfactor = device_scale_factor ? device_scale_factor /
                                                screen_info.device_scale_factor
                                          : 1;
    params.viewport_scale = viewport.fromJust()->GetScale() * dpfactor;

    // Resize the RenderWidgetHostView to the size of the overridden viewport.
    width = gfx::ToRoundedInt(viewport.fromJust()->GetWidth() *
                              params.viewport_scale);
    height = gfx::ToRoundedInt(viewport.fromJust()->GetHeight() *
                               params.viewport_scale);
  }

  bool size_changed = false;
  if (!dont_set_visible_size.fromMaybe(false) && width > 0 && height > 0) {
    if (GetWebContents()) {
      size_changed =
          GetWebContents()->SetDeviceEmulationSize(gfx::Size(width, height));
    } else {
      return Response::Error("Can't find the associated web contents");
    }
  }

  if (device_emulation_enabled_ && params == device_emulation_params_) {
    // Renderer should answer after size was changed, so that the response is
    // only sent to the client once updates were applied.
    if (size_changed)
      return Response::FallThrough();
    return Response::OK();
  }

  device_emulation_enabled_ = true;
  device_emulation_params_ = params;
  UpdateDeviceEmulationState();
  // Renderer should answer after emulation params were updated, so that the
  // response is only sent to the client once updates were applied.
  return Response::FallThrough();
}

Response EmulationHandler::ClearDeviceMetricsOverride() {
  if (!device_emulation_enabled_)
    return Response::OK();
  if (GetWebContents())
    GetWebContents()->ClearDeviceEmulationSize();
  else
    return Response::Error("Can't find the associated web contents");
  device_emulation_enabled_ = false;
  device_emulation_params_ = blink::WebDeviceEmulationParams();
  UpdateDeviceEmulationState();
  // Renderer should answer after emulation was disabled, so that the response
  // is only sent to the client once updates were applied.
  return Response::FallThrough();
}

Response EmulationHandler::SetVisibleSize(int width, int height) {
  if (width < 0 || height < 0)
    return Response::InvalidParams("Width and height must be non-negative");

  if (GetWebContents())
    GetWebContents()->SetDeviceEmulationSize(gfx::Size(width, height));
  else
    return Response::Error("Can't find the associated web contents");

  return Response::OK();
}

blink::WebDeviceEmulationParams EmulationHandler::GetDeviceEmulationParams() {
  return device_emulation_params_;
}

void EmulationHandler::SetDeviceEmulationParams(
    const blink::WebDeviceEmulationParams& params) {
  bool enabled = params != blink::WebDeviceEmulationParams();
  device_emulation_enabled_ = enabled;
  device_emulation_params_ = params;
  UpdateDeviceEmulationState();
}

WebContentsImpl* EmulationHandler::GetWebContents() {
  return host_ ?
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_)) :
      nullptr;
}

void EmulationHandler::UpdateTouchEventEmulationState() {
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return;
  if (touch_emulation_enabled_) {
    widget_host->GetTouchEmulator()->Enable(
        TouchEmulator::Mode::kEmulatingTouchFromMouse,
        TouchEmulationConfigurationToType(touch_emulation_configuration_));
  } else {
    widget_host->GetTouchEmulator()->Disable();
  }
  if (GetWebContents()) {
    GetWebContents()->SetForceDisableOverscrollContent(
        touch_emulation_enabled_);
  }
}

void EmulationHandler::UpdateDeviceEmulationState() {
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return;
  // TODO(eseckler): Once we change this to mojo, we should wait for an ack to
  // these messages from the renderer. The renderer should send the ack once the
  // emulation params were applied. That way, we can avoid having to handle
  // Set/ClearDeviceMetricsOverride in the renderer. With the old IPC system,
  // this is tricky since we'd have to track the DevTools message id with the
  // ViewMsg and acknowledgment, as well as plump the acknowledgment back to the
  // EmulationHandler somehow. Mojo callbacks should make this much simpler.
  if (device_emulation_enabled_) {
    widget_host->Send(new ViewMsg_EnableDeviceEmulation(
        widget_host->GetRoutingID(), device_emulation_params_));
  } else {
    widget_host->Send(new ViewMsg_DisableDeviceEmulation(
        widget_host->GetRoutingID()));
  }
}

}  // namespace protocol
}  // namespace content
