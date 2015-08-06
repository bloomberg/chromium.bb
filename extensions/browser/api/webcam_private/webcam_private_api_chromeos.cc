// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/webcam_private_api.h"

#include "base/lazy_instance.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/api/webcam_private/v4l2_webcam.h"
#include "extensions/browser/api/webcam_private/visca_webcam.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/api/webcam_private.h"

namespace webcam_private = extensions::api::webcam_private;

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
      weak_ptr_factory_(this) {
  webcam_resource_manager_.reset(
      new ApiResourceManager<WebcamResource>(context));
}

WebcamPrivateAPI::~WebcamPrivateAPI() {
}

Webcam* WebcamPrivateAPI::GetWebcam(const std::string& extension_id,
                                    const std::string& webcam_id) {
  WebcamResource* webcam_resource = FindWebcamResource(extension_id, webcam_id);
  if (webcam_resource)
    return webcam_resource->GetWebcam();

  std::string device_id;
  GetDeviceId(extension_id, webcam_id, &device_id);
  V4L2Webcam* v4l2_webcam(new V4L2Webcam(device_id));
  if (!v4l2_webcam->Open()) {
    return nullptr;
  }

  webcam_resource_manager_->Add(
      new WebcamResource(extension_id, v4l2_webcam, webcam_id));

  return v4l2_webcam;
}

bool WebcamPrivateAPI::OpenSerialWebcam(
    const std::string& extension_id,
    const std::string& device_path,
    const base::Callback<void(const std::string&, bool)>& callback) {
  const std::string& webcam_id = GetWebcamId(extension_id, device_path);
  WebcamResource* webcam_resource = FindWebcamResource(extension_id, webcam_id);
  if (webcam_resource)
    return false;

  ViscaWebcam* visca_webcam(new ViscaWebcam(device_path, extension_id));
  visca_webcam->Open(base::Bind(
      &WebcamPrivateAPI::OnOpenSerialWebcam, weak_ptr_factory_.GetWeakPtr(),
      extension_id, device_path, make_scoped_refptr(visca_webcam), callback));
  return true;
}

bool WebcamPrivateAPI::CloseWebcam(const std::string& extension_id,
                                   const std::string& webcam_id) {
  if (FindWebcamResource(extension_id, webcam_id)) {
    RemoveWebcamResource(extension_id, webcam_id);
    return true;
  }
  return false;
}

void WebcamPrivateAPI::OnOpenSerialWebcam(
    const std::string& extension_id,
    const std::string& device_path,
    scoped_refptr<Webcam> webcam,
    const base::Callback<void(const std::string&, bool)>& callback,
    bool success) {
  if (success) {
    const std::string& webcam_id = GetWebcamId(extension_id, device_path);
    webcam_resource_manager_->Add(
        new WebcamResource(extension_id, webcam.get(), webcam_id));
    callback.Run(webcam_id, true);
  } else {
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

WebcamResource* WebcamPrivateAPI::FindWebcamResource(
    const std::string& extension_id,
    const std::string& webcam_id) const {
  DCHECK(webcam_resource_manager_);

  base::hash_set<int>* connection_ids =
      webcam_resource_manager_->GetResourceIds(extension_id);
  if (!connection_ids)
    return nullptr;

  for (const auto& connection_id : *connection_ids) {
    WebcamResource* webcam_resource =
        webcam_resource_manager_->Get(extension_id, connection_id);
    if (webcam_resource && webcam_resource->GetWebcamId() == webcam_id)
      return webcam_resource;
  }

  return nullptr;
}

bool WebcamPrivateAPI::RemoveWebcamResource(const std::string& extension_id,
                                            const std::string& webcam_id) {
  DCHECK(webcam_resource_manager_);

  base::hash_set<int>* connection_ids =
      webcam_resource_manager_->GetResourceIds(extension_id);
  if (!connection_ids)
    return false;

  for (const auto& connection_id : *connection_ids) {
    WebcamResource* webcam_resource =
        webcam_resource_manager_->Get(extension_id, connection_id);
    if (webcam_resource && webcam_resource->GetWebcamId() == webcam_id) {
      webcam_resource_manager_->Remove(extension_id, connection_id);
      return true;
    }
  }

  return false;
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
      ->CloseWebcam(extension_id(), params->webcam_id);
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
                       ->GetWebcam(extension_id(), params->webcam_id);
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
                       ->GetWebcam(extension_id(), params->webcam_id);
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
                       ->GetWebcam(extension_id(), params->webcam_id);
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
