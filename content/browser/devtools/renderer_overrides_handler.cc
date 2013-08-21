// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

namespace content {

namespace {

static const char kPng[] = "png";
static const char kJpeg[] = "jpeg";
static int kDefaultScreenshotQuality = 80;

void ParseCaptureParameters(DevToolsProtocol::Command* command,
                            std::string* format,
                            int* quality,
                            double* scale) {
  *quality = kDefaultScreenshotQuality;
  *scale = 1;
  base::DictionaryValue* params = command->params();
  if (params) {
    params->GetString(devtools::Page::captureScreenshot::kParamFormat,
                      format);
    params->GetInteger(devtools::Page::captureScreenshot::kParamQuality,
                       quality);
    params->GetDouble(devtools::Page::captureScreenshot::kParamScale,
                      scale);
  }
  if (format->empty())
    *format = kPng;
  if (*quality < 0 || *quality > 100)
    *quality = kDefaultScreenshotQuality;
  if (*scale <= 0 || *scale > 1)
    *scale = 1;
}

void ParseGenericInputParams(base::DictionaryValue* params,
                             WebInputEvent* event) {
  int modifiers = 0;
  if (params->GetInteger(devtools::Input::kParamModifiers,
                         &modifiers)) {
    if (modifiers & 1)
      event->modifiers |= WebInputEvent::AltKey;
    if (modifiers & 2)
      event->modifiers |= WebInputEvent::ControlKey;
    if (modifiers & 4)
      event->modifiers |= WebInputEvent::MetaKey;
    if (modifiers & 8)
      event->modifiers |= WebInputEvent::ShiftKey;
  }

  params->GetDouble(devtools::Input::kParamTimestamp,
                    &event->timeStampSeconds);
}

}  // namespace

RendererOverridesHandler::RendererOverridesHandler(DevToolsAgentHost* agent)
    : agent_(agent),
      weak_factory_(this) {
  RegisterCommandHandler(
      devtools::DOM::setFileInputFiles::kName,
      base::Bind(
          &RendererOverridesHandler::GrantPermissionsForSetFileInputFiles,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::disable::kName,
      base::Bind(
          &RendererOverridesHandler::PageDisable, base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::handleJavaScriptDialog::kName,
      base::Bind(
          &RendererOverridesHandler::PageHandleJavaScriptDialog,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::navigate::kName,
      base::Bind(
          &RendererOverridesHandler::PageNavigate,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::captureScreenshot::kName,
      base::Bind(
          &RendererOverridesHandler::PageCaptureScreenshot,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::startScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageStartScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::stopScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageStopScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Input::dispatchMouseEvent::kName,
      base::Bind(
          &RendererOverridesHandler::InputDispatchMouseEvent,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Input::dispatchGestureEvent::kName,
      base::Bind(
          &RendererOverridesHandler::InputDispatchGestureEvent,
          base::Unretained(this)));
}

RendererOverridesHandler::~RendererOverridesHandler() {}

void RendererOverridesHandler::OnClientDetached() {
  screencast_command_ = NULL;
}

void RendererOverridesHandler::OnSwapCompositorFrame() {
  if (!screencast_command_)
    return;

  std::string format;
  int quality = kDefaultScreenshotQuality;
  double scale = 1;
  ParseCaptureParameters(screencast_command_.get(), &format, &quality, &scale);

  RenderViewHost* host = agent_->GetRenderViewHost();
  RenderWidgetHostViewPort* view_port =
      RenderWidgetHostViewPort::FromRWHV(host->GetView());

  gfx::Rect view_bounds = host->GetView()->GetViewBounds();
  gfx::Size snapshot_size = gfx::ToFlooredSize(
      gfx::ScaleSize(view_bounds.size(), scale));

  view_port->CopyFromCompositingSurface(
      view_bounds, snapshot_size,
      base::Bind(&RendererOverridesHandler::ScreenshotCaptured,
                 weak_factory_.GetWeakPtr(),
                 scoped_refptr<DevToolsProtocol::Command>(), format, quality,
                 scale));
}


// DOM agent handlers  --------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::GrantPermissionsForSetFileInputFiles(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  base::ListValue* file_list = NULL;
  const char* param =
      devtools::DOM::setFileInputFiles::kParamFiles;
  if (!params || !params->GetList(param, &file_list))
    return command->InvalidParamResponse(param);
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (!host)
    return NULL;

  for (size_t i = 0; i < file_list->GetSize(); ++i) {
    base::FilePath::StringType file;
    if (!file_list->GetString(i, &file))
      return command->InvalidParamResponse(param);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        host->GetProcess()->GetID(), base::FilePath(file));
  }
  return NULL;
}


// Page agent handlers  -------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageDisable(
    scoped_refptr<DevToolsProtocol::Command> command) {
  screencast_command_ = NULL;
  return NULL;
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageHandleJavaScriptDialog(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  const char* paramAccept =
      devtools::Page::handleJavaScriptDialog::kParamAccept;
  bool accept;
  if (!params || !params->GetBoolean(paramAccept, &accept))
    return command->InvalidParamResponse(paramAccept);
  string16 prompt_override;
  string16* prompt_override_ptr = &prompt_override;
  if (!params || !params->GetString(
      devtools::Page::handleJavaScriptDialog::kParamPromptText,
      prompt_override_ptr)) {
    prompt_override_ptr = NULL;
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      JavaScriptDialogManager* manager =
          web_contents->GetDelegate()->GetJavaScriptDialogManager();
      if (manager && manager->HandleJavaScriptDialog(
              web_contents, accept, prompt_override_ptr)) {
        return NULL;
      }
    }
  }
  return command->InternalErrorResponse("No JavaScript dialog to handle");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageNavigate(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  std::string url;
  const char* param = devtools::Page::navigate::kParamUrl;
  if (!params || !params->GetString(param, &url))
    return command->InvalidParamResponse(param);
  GURL gurl(url);
  if (!gurl.is_valid()) {
    return command->InternalErrorResponse("Cannot navigate to invalid URL");
  }
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      web_contents->GetController()
          .LoadURL(gurl, Referrer(), PAGE_TRANSITION_TYPED, std::string());
      return command->SuccessResponse(new base::DictionaryValue());
    }
  }
  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageCaptureScreenshot(
    scoped_refptr<DevToolsProtocol::Command> command) {
  std::string format;
  int quality = kDefaultScreenshotQuality;
  double scale = 1;
  ParseCaptureParameters(command.get(), &format, &quality, &scale);

  RenderViewHost* host = agent_->GetRenderViewHost();
  gfx::Rect view_bounds = host->GetView()->GetViewBounds();

  // Grab screen pixels if available for current platform.
  // TODO(pfeldman): support format, scale and quality in ui::GrabViewSnapshot.
  std::vector<unsigned char> png;
  bool is_unscaled_png = scale == 1 && format == kPng;
  if (is_unscaled_png && ui::GrabViewSnapshot(host->GetView()->GetNativeView(),
                                              &png, view_bounds)) {
    std::string base64_data;
    bool success = base::Base64Encode(
        base::StringPiece(reinterpret_cast<char*>(&*png.begin()), png.size()),
        &base64_data);
    if (success) {
      base::DictionaryValue* result = new base::DictionaryValue();
      result->SetString(
          devtools::Page::captureScreenshot::kResponseData, base64_data);
      return command->SuccessResponse(result);
    }
    return command->InternalErrorResponse("Unable to base64encode screenshot");
  }

  // Fallback to copying from compositing surface.
  RenderWidgetHostViewPort* view_port =
      RenderWidgetHostViewPort::FromRWHV(host->GetView());

  gfx::Size snapshot_size = gfx::ToFlooredSize(
      gfx::ScaleSize(view_bounds.size(), scale));
  view_port->CopyFromCompositingSurface(
      view_bounds, snapshot_size,
      base::Bind(&RendererOverridesHandler::ScreenshotCaptured,
                 weak_factory_.GetWeakPtr(), command, format, quality, scale));
  return command->AsyncResponsePromise();
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageStartScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  screencast_command_ = command;
  OnSwapCompositorFrame();
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageStopScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  screencast_command_ = NULL;
  return command->SuccessResponse(NULL);
}

void RendererOverridesHandler::ScreenshotCaptured(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::string& format,
    int quality,
    double scale,
    bool success,
    const SkBitmap& bitmap) {
  if (!success) {
    if (command) {
      SendAsyncResponse(
          command->InternalErrorResponse("Unable to capture screenshot"));
    }
    return;
  }

  std::vector<unsigned char> data;
  SkAutoLockPixels lock_image(bitmap);
  bool encoded;
  if (format == kPng) {
    encoded = gfx::PNGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::PNGCodec::FORMAT_SkBitmap,
        gfx::Size(bitmap.width(), bitmap.height()),
        bitmap.width() * bitmap.bytesPerPixel(),
        false, std::vector<gfx::PNGCodec::Comment>(), &data);
  } else if (format == kJpeg) {
    encoded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bitmap.width(),
        bitmap.height(),
        bitmap.width() * bitmap.bytesPerPixel(),
        quality, &data);
  } else {
    encoded = false;
  }

  if (!encoded) {
    if (command) {
      SendAsyncResponse(
          command->InternalErrorResponse("Unable to encode screenshot"));
    }
    return;
  }

  std::string base_64_data;
  if (!base::Base64Encode(base::StringPiece(
                              reinterpret_cast<char*>(&data[0]),
                              data.size()),
                          &base_64_data)) {
    if (command) {
      SendAsyncResponse(
          command->InternalErrorResponse("Unable to base64 encode"));
    }
    return;
  }

  base::DictionaryValue* response = new base::DictionaryValue();
  if (command) {
    response->SetString(
        devtools::Page::captureScreenshot::kResponseData, base_64_data);
    SendAsyncResponse(command->SuccessResponse(response));
  } else {
    response->SetString(
        devtools::Page::screencastFrame::kResponseData, base_64_data);
    SendNotification(devtools::Page::screencastFrame::kName, response);
  }
}


// Input agent handlers  ------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::InputDispatchMouseEvent(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  if (!params)
    return NULL;

  bool device_space = false;
  if (!params->GetBoolean(devtools::Input::kParamDeviceSpace,
                          &device_space) ||
      !device_space) {
    return NULL;
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  WebKit::WebMouseEvent mouse_event;
  ParseGenericInputParams(params, &mouse_event);

  std::string type;
  if (params->GetString(devtools::Input::kParamType,
                        &type)) {
    if (type == "mousePressed")
      mouse_event.type = WebInputEvent::MouseDown;
    else if (type == "mouseReleased")
      mouse_event.type = WebInputEvent::MouseUp;
    else if (type == "mouseMoved")
      mouse_event.type = WebInputEvent::MouseMove;
    else
      return NULL;
  } else {
    return NULL;
  }

  int x;
  int y;
  if (!params->GetInteger(devtools::Input::kParamX, &x) ||
      !params->GetInteger(devtools::Input::kParamY, &y)) {
    return NULL;
  }

  float device_scale_factor = ui::GetScaleFactorScale(
      GetScaleFactorForView(host->GetView()));
  mouse_event.x = floor(x / device_scale_factor);
  mouse_event.y = floor(y / device_scale_factor);
  mouse_event.windowX = mouse_event.x;
  mouse_event.windowY = mouse_event.y;
  mouse_event.globalX = mouse_event.x;
  mouse_event.globalY = mouse_event.y;

  params->GetInteger(devtools::Input::dispatchMouseEvent::kParamClickCount,
                     &mouse_event.clickCount);

  std::string button;
  if (!params->GetString(devtools::Input::dispatchMouseEvent::kParamButton,
                         &button)) {
    return NULL;
  }

  if (button == "none") {
    mouse_event.button = WebMouseEvent::ButtonNone;
  } else if (button == "left") {
    mouse_event.button = WebMouseEvent::ButtonLeft;
    mouse_event.modifiers |= WebInputEvent::LeftButtonDown;
  } else if (button == "middle") {
    mouse_event.button = WebMouseEvent::ButtonMiddle;
    mouse_event.modifiers |= WebInputEvent::MiddleButtonDown;
  } else if (button == "right") {
    mouse_event.button = WebMouseEvent::ButtonRight;
    mouse_event.modifiers |= WebInputEvent::RightButtonDown;
  } else {
    return NULL;
  }

  host->ForwardMouseEvent(mouse_event);
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::InputDispatchGestureEvent(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  if (!params)
    return NULL;

  RenderViewHostImpl* host = static_cast<RenderViewHostImpl*>(
      agent_->GetRenderViewHost());
  WebKit::WebGestureEvent event;
  ParseGenericInputParams(params, &event);

  std::string type;
  if (params->GetString(devtools::Input::kParamType,
                        &type)) {
    if (type == "scrollBegin")
      event.type = WebInputEvent::GestureScrollBegin;
    else if (type == "scrollUpdate")
      event.type = WebInputEvent::GestureScrollUpdate;
    else if (type == "scrollEnd")
      event.type = WebInputEvent::GestureScrollEnd;
    else if (type == "tapDown")
      event.type = WebInputEvent::GestureTapDown;
    else if (type == "tap")
      event.type = WebInputEvent::GestureTap;
    else if (type == "pinchBegin")
      event.type = WebInputEvent::GesturePinchBegin;
    else if (type == "pinchUpdate")
      event.type = WebInputEvent::GesturePinchUpdate;
    else if (type == "pinchEnd")
      event.type = WebInputEvent::GesturePinchEnd;
    else
      return NULL;
  } else {
    return NULL;
  }

  float device_scale_factor = ui::GetScaleFactorScale(
      GetScaleFactorForView(host->GetView()));

  int x;
  int y;
  if (!params->GetInteger(devtools::Input::kParamX, &x) ||
      !params->GetInteger(devtools::Input::kParamY, &y)) {
    return NULL;
  }
  event.x = floor(x / device_scale_factor);
  event.y = floor(y / device_scale_factor);
  event.globalX = event.x;
  event.globalY = event.y;

  if (type == "scrollUpdate") {
    int dx;
    int dy;
    if (!params->GetInteger(
            devtools::Input::dispatchGestureEvent::kParamDeltaX, &dx) ||
        !params->GetInteger(
            devtools::Input::dispatchGestureEvent::kParamDeltaY, &dy)) {
      return NULL;
    }
    event.data.scrollUpdate.deltaX = floor(dx / device_scale_factor);
    event.data.scrollUpdate.deltaY = floor(dy / device_scale_factor);
  }

  if (type == "pinchUpdate") {
    double scale;
    if (!params->GetDouble(
        devtools::Input::dispatchGestureEvent::kParamPinchScale,
        &scale)) {
      return NULL;
    }
    event.data.pinchUpdate.scale = static_cast<float>(scale);
  }

  host->ForwardGestureEvent(event);
  return command->SuccessResponse(NULL);
}

}  // namespace content
