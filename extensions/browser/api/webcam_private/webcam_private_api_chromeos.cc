// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/webcam_private_api.h"

#include "base/lazy_instance.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/browser/api/webcam_private/v4l2_webcam.h"
#include "extensions/browser/api/webcam_private/visca_webcam.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/api/webcam_private.h"

namespace webcam_private = extensions::core_api::webcam_private;

namespace content {
class BrowserContext;
}  // namespace content

namespace {
const char kPathInUse[] = "Path in use";
const char kUnknownWebcam[] = "Unknown webcam id";
}  // namespace

namespace extensions {

// static
WebcamPrivateAPI* WebcamPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

WebcamPrivateAPI::WebcamPrivateAPI(content::BrowserContext* context)
    : browser_context_(context),
      process_manager_observer_(this),
      weak_ptr_factory_(this) {
  process_manager_observer_.Add(ProcessManager::Get(browser_context_));
}

WebcamPrivateAPI::~WebcamPrivateAPI() {}

Webcam* WebcamPrivateAPI::GetWebcam(const std::string& extension_id,
                                    const std::string& device_id) {
  auto ix = webcams_.find(device_id);
  if (ix != webcams_.end()) {
    ix->second->AddExtensionRef(extension_id);
    return ix->second.get();
  }

  scoped_ptr<V4L2Webcam> v4l2_webcam(new V4L2Webcam(device_id));
  if (!v4l2_webcam->Open()) {
    return nullptr;
  }

  linked_ptr<Webcam> webcam(v4l2_webcam.release());

  webcams_[device_id] = webcam;
  webcam->AddExtensionRef(extension_id);

  return webcam.get();
}

bool WebcamPrivateAPI::OpenSerialWebcam(
    const std::string& extension_id,
    const std::string& device_id,
    const base::Callback<void(const std::string&, bool)>& callback) {
  auto ix = webcams_.find(device_id);
  if (ix != webcams_.end()) {
    return false;
  }

  ViscaWebcam* visca_webcam(new ViscaWebcam(device_id, extension_id));
  visca_webcam->AddExtensionRef(extension_id);
  webcams_[device_id] = linked_ptr<Webcam>(visca_webcam);

  visca_webcam->Open(base::Bind(&WebcamPrivateAPI::OnOpenSerialWebcam,
                                weak_ptr_factory_.GetWeakPtr(), extension_id,
                                device_id, callback));
  return true;
}

bool WebcamPrivateAPI::CloseWebcam(const std::string& extension_id,
                                   const std::string& device_id) {
  auto webcam = webcams_.find(device_id);
  if (webcam == webcams_.end())
    return false;

  webcam->second->RemoveExtensionRef(extension_id);
  if (webcam->second->ShouldDelete())
    webcams_.erase(webcam);
  return true;
}

void WebcamPrivateAPI::OnOpenSerialWebcam(
    const std::string& extension_id,
    const std::string& device_id,
    const base::Callback<void(const std::string&, bool)>& callback,
    bool success) {
  if (success) {
    std::string webcam_id = GetWebcamId(extension_id, device_id);
    callback.Run(webcam_id, true);
  } else {
    webcams_.erase(device_id);
    callback.Run("", false);
  }
}

bool WebcamPrivateAPI::GetDeviceId(const std::string& extension_id,
                                   const std::string& webcam_id,
                                   std::string* device_id) {
  GURL security_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);

  return content::GetMediaDeviceIDForHMAC(
      content::MEDIA_DEVICE_VIDEO_CAPTURE,
      browser_context_->GetResourceContext()->GetMediaDeviceIDSalt(),
      security_origin,
      webcam_id,
      device_id);
}

std::string WebcamPrivateAPI::GetWebcamId(const std::string& extension_id,
                                          const std::string& device_id) {
  GURL security_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);

  return content::GetHMACForMediaDeviceID(
      browser_context_->GetResourceContext()->GetMediaDeviceIDSalt(),
      security_origin, device_id);
}

void WebcamPrivateAPI::OnBackgroundHostClose(const std::string& extension_id) {
  for (auto webcam = webcams_.begin();
       webcam != webcams_.end(); /* No increment */ ) {
    auto next = std::next(webcam);
    webcam->second->RemoveExtensionRef(extension_id);
    if (webcam->second->ShouldDelete())
      webcams_.erase(webcam);
    webcam = next;
  }
}

WebcamPrivateOpenSerialWebcamFunction::WebcamPrivateOpenSerialWebcamFunction() {
}

WebcamPrivateOpenSerialWebcamFunction::
    ~WebcamPrivateOpenSerialWebcamFunction() {
}

bool WebcamPrivateOpenSerialWebcamFunction::RunAsync() {
  scoped_ptr<webcam_private::OpenSerialWebcam::Params> params(
      webcam_private::OpenSerialWebcam::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (WebcamPrivateAPI::Get(browser_context())
          ->OpenSerialWebcam(
              extension_id(), params->path,
              base::Bind(&WebcamPrivateOpenSerialWebcamFunction::OnOpenWebcam,
                         this))) {
    return true;
  } else {
    SetError(kPathInUse);
    return false;
  }
}

void WebcamPrivateOpenSerialWebcamFunction::OnOpenWebcam(
    const std::string& webcam_id,
    bool success) {
  if (success) {
    SetResult(new base::StringValue(webcam_id));
    SendResponse(true);
  } else {
    SetError(kUnknownWebcam);
    SendResponse(false);
  }
}

WebcamPrivateCloseWebcamFunction::WebcamPrivateCloseWebcamFunction() {
}

WebcamPrivateCloseWebcamFunction::~WebcamPrivateCloseWebcamFunction() {
}

bool WebcamPrivateCloseWebcamFunction::RunAsync() {
  scoped_ptr<webcam_private::CloseWebcam::Params> params(
      webcam_private::CloseWebcam::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return WebcamPrivateAPI::Get(browser_context())
      ->CloseWebcam(extension_id(), params->path);
}

WebcamPrivateSetFunction::WebcamPrivateSetFunction() {
}

WebcamPrivateSetFunction::~WebcamPrivateSetFunction() {
}

bool WebcamPrivateSetFunction::RunSync() {
  scoped_ptr<webcam_private::Set::Params> params(
      webcam_private::Set::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Webcam* webcam = WebcamPrivateAPI::Get(browser_context())
                       ->GetWebcam(extension_id(), params->path);
  if (!webcam) {
    SetError(kUnknownWebcam);
    return false;
  }

  if (params->config.pan) {
    webcam->SetPan(*(params->config.pan));
  }

  if (params->config.pan_direction) {
    Webcam::PanDirection direction = Webcam::PAN_STOP;
    switch (params->config.pan_direction) {
      case webcam_private::PAN_DIRECTION_NONE:
      case webcam_private::PAN_DIRECTION_STOP:
        direction = Webcam::PAN_STOP;
        break;

      case webcam_private::PAN_DIRECTION_RIGHT:
        direction = Webcam::PAN_RIGHT;
        break;

      case webcam_private::PAN_DIRECTION_LEFT:
        direction = Webcam::PAN_LEFT;
        break;
    }
    webcam->SetPanDirection(direction);
  }

  if (params->config.tilt) {
    webcam->SetTilt(*(params->config.tilt));
  }

  if (params->config.tilt_direction) {
    Webcam::TiltDirection direction = Webcam::TILT_STOP;
    switch (params->config.tilt_direction) {
      case webcam_private::TILT_DIRECTION_NONE:
      case webcam_private::TILT_DIRECTION_STOP:
        direction = Webcam::TILT_STOP;
        break;

      case webcam_private::TILT_DIRECTION_UP:
        direction = Webcam::TILT_UP;
        break;

      case webcam_private::TILT_DIRECTION_DOWN:
        direction = Webcam::TILT_DOWN;
        break;
    }
    webcam->SetTiltDirection(direction);
  }

  if (params->config.zoom) {
    webcam->SetZoom(*(params->config.zoom));
  }


  return true;
}

WebcamPrivateGetFunction::WebcamPrivateGetFunction() {
}

WebcamPrivateGetFunction::~WebcamPrivateGetFunction() {
}

bool WebcamPrivateGetFunction::RunSync() {
  scoped_ptr<webcam_private::Get::Params> params(
      webcam_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Webcam* webcam = WebcamPrivateAPI::Get(browser_context())
                       ->GetWebcam(extension_id(), params->path);
  if (!webcam) {
    SetError(kUnknownWebcam);
    return false;
  }

  webcam_private::WebcamConfiguration result;

  int pan;
  if (webcam->GetPan(&pan))
    result.pan.reset(new double(pan));

  int tilt;
  if (webcam->GetTilt(&tilt))
    result.tilt.reset(new double(tilt));

  int zoom;
  if (webcam->GetZoom(&zoom))
    result.zoom.reset(new double(zoom));

  SetResult(result.ToValue().release());

  return true;
}

WebcamPrivateResetFunction::WebcamPrivateResetFunction() {
}

WebcamPrivateResetFunction::~WebcamPrivateResetFunction() {
}

bool WebcamPrivateResetFunction::RunSync() {
  scoped_ptr<webcam_private::Reset::Params> params(
      webcam_private::Reset::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Webcam* webcam = WebcamPrivateAPI::Get(browser_context())
                       ->GetWebcam(extension_id(), params->path);
  if (!webcam) {
    SetError(kUnknownWebcam);
    return false;
  }

  webcam->Reset(params->config.pan, params->config.tilt, params->config.zoom);

  return true;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<WebcamPrivateAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<WebcamPrivateAPI>*
WebcamPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<WebcamPrivateAPI>
    ::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ProcessManagerFactory::GetInstance());
}

}  // namespace extensions
