// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/webcam_private_api.h"

#include <fcntl.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/api/webcam_private.h"

#define V4L2_CID_PAN_SPEED (V4L2_CID_CAMERA_CLASS_BASE+32)
#define V4L2_CID_TILT_SPEED (V4L2_CID_CAMERA_CLASS_BASE+33)
#define V4L2_CID_PANTILT_CMD (V4L2_CID_CAMERA_CLASS_BASE+34)

// GUID of the Extension Unit for Logitech CC3300e motor control:
// {212de5ff-3080-2c4e-82d9-f587d00540bd}
#define UVC_GUID_LOGITECH_CC3000E_MOTORS          \
 {0x21, 0x2d, 0xe5, 0xff, 0x30, 0x80, 0x2c, 0x4e, \
  0x82, 0xd9, 0xf5, 0x87, 0xd0, 0x05, 0x40, 0xbd}

#define LOGITECH_MOTORCONTROL_PANTILT_CMD 2

namespace webcam_private = extensions::core_api::webcam_private;

namespace content {
class BrowserContext;
}  // namespace content

namespace {
const int kLogitechMenuIndexGoHome = 2;

const uvc_menu_info kLogitechCmdMenu[] = {
  {1, "Set Preset"}, {2, "Get Preset"}, {3, "Go Home"}
};

const uvc_xu_control_mapping kLogitechCmdMapping = {
  V4L2_CID_PANTILT_CMD,
  "Pan/Tilt Go",
  UVC_GUID_LOGITECH_CC3000E_MOTORS,
  LOGITECH_MOTORCONTROL_PANTILT_CMD,
  8,
  0,
  V4L2_CTRL_TYPE_MENU,
  UVC_CTRL_DATA_TYPE_ENUM,
  const_cast<uvc_menu_info*>(&kLogitechCmdMenu[0]),
  arraysize(kLogitechCmdMenu),
};

base::ScopedFD OpenWebcam(const std::string& extension_id,
                          content::BrowserContext* browser_context,
                          const std::string& webcam_id) {
  GURL security_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);

  std::string device_id;
  bool success = content::GetMediaDeviceIDForHMAC(
      content::MEDIA_DEVICE_VIDEO_CAPTURE,
      browser_context->GetResourceContext()->GetMediaDeviceIDSalt(),
      security_origin,
      webcam_id,
      &device_id);

  if (!success)
    return base::ScopedFD();

  return base::ScopedFD(HANDLE_EINTR(open(device_id.c_str(), 0)));
}

void SetWebcamParameter(int fd, uint32_t control_id, int value) {
  struct v4l2_control v4l2_ctrl = {control_id, value};
  HANDLE_EINTR(ioctl(fd, VIDIOC_S_CTRL, &v4l2_ctrl));
}

bool GetWebcamParameter(int fd, uint32_t control_id, int* value) {
  struct v4l2_control v4l2_ctrl = {control_id};

  if (HANDLE_EINTR(ioctl(fd, VIDIOC_G_CTRL, &v4l2_ctrl)))
    return false;

  *value = v4l2_ctrl.value;
  return true;
}

bool EnsureLogitechCommandsMapped(int fd) {
  int res = ioctl(fd, UVCIOC_CTRL_MAP, &kLogitechCmdMapping);
  // If mapping is successful or it's already mapped, this is a Logitech camera.
  return res >= 0 || errno == EEXIST;
}

const char kUnknownWebcam[] = "Unknown webcam id";
}  // namespace

namespace extensions {

WebcamPrivateSetFunction::WebcamPrivateSetFunction() {
}

WebcamPrivateSetFunction::~WebcamPrivateSetFunction() {
}

bool WebcamPrivateSetFunction::RunSync() {
  // Get parameters
  scoped_ptr<webcam_private::Set::Params> params(
      webcam_private::Set::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ScopedFD fd =
      OpenWebcam(extension_id(), browser_context(), params->webcam_id);
  if (!fd.is_valid()) {
    SetError(kUnknownWebcam);
    return false;
  }

  if (params->config.pan) {
    SetWebcamParameter(fd.get(), V4L2_CID_PAN_ABSOLUTE,
                       *(params->config.pan));
  }

  if (params->config.pan_direction) {
    int direction = 0;
    switch (params->config.pan_direction) {
      case webcam_private::PAN_DIRECTION_NONE:
      case webcam_private::PAN_DIRECTION_STOP:
        direction = 0;
        break;

      case webcam_private::PAN_DIRECTION_RIGHT:
        direction = 1;
        break;

      case webcam_private::PAN_DIRECTION_LEFT:
        direction = -1;
        break;
    }
    SetWebcamParameter(fd.get(), V4L2_CID_PAN_SPEED, direction);
  }

  if (params->config.tilt) {
    SetWebcamParameter(fd.get(), V4L2_CID_TILT_ABSOLUTE,
                       *(params->config.tilt));
  }

  if (params->config.tilt_direction) {
    int direction = 0;
    switch (params->config.tilt_direction) {
      case webcam_private::TILT_DIRECTION_NONE:
      case webcam_private::TILT_DIRECTION_STOP:
        direction = 0;
        break;

      case webcam_private::TILT_DIRECTION_UP:
        direction = 1;
        break;

      case webcam_private::TILT_DIRECTION_DOWN:
        direction = -1;
        break;
    }
    SetWebcamParameter(fd.get(), V4L2_CID_TILT_SPEED, direction);
  }

  if (params->config.zoom) {
    SetWebcamParameter(fd.get(), V4L2_CID_ZOOM_ABSOLUTE,
                       *(params->config.zoom));
  }


  return true;
}

WebcamPrivateGetFunction::WebcamPrivateGetFunction() {
}

WebcamPrivateGetFunction::~WebcamPrivateGetFunction() {
}

bool WebcamPrivateGetFunction::RunSync() {
  // Get parameters
  scoped_ptr<webcam_private::Get::Params> params(
      webcam_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ScopedFD fd =
      OpenWebcam(extension_id(), browser_context(), params->webcam_id);
  if (!fd.is_valid()) {
    SetError(kUnknownWebcam);
    return false;
  }

  webcam_private::WebcamConfiguration result;

  int pan;
  if (GetWebcamParameter(fd.get(), V4L2_CID_PAN_ABSOLUTE, &pan))
    result.pan.reset(new double(pan));

  int tilt;
  if (GetWebcamParameter(fd.get(), V4L2_CID_TILT_ABSOLUTE, &tilt))
    result.tilt.reset(new double(tilt));

  int zoom;
  if (GetWebcamParameter(fd.get(), V4L2_CID_ZOOM_ABSOLUTE, &zoom))
    result.zoom.reset(new double(zoom));

  SetResult(result.ToValue().release());

  return true;
}

WebcamPrivateResetFunction::WebcamPrivateResetFunction() {
}

WebcamPrivateResetFunction::~WebcamPrivateResetFunction() {
}

bool WebcamPrivateResetFunction::RunSync() {
  // Get parameters
  scoped_ptr<webcam_private::Reset::Params> params(
      webcam_private::Reset::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ScopedFD fd =
      OpenWebcam(extension_id(), browser_context(), params->webcam_id);
  if (!fd.is_valid()) {
    SetError(kUnknownWebcam);
    return false;
  }

  if (params->config.pan || params->config.tilt) {
    if (EnsureLogitechCommandsMapped(fd.get())) {
      SetWebcamParameter(fd.get(), V4L2_CID_PANTILT_CMD,
                         kLogitechMenuIndexGoHome);
    }
  }

  if (params->config.pan) {
    struct v4l2_control v4l2_ctrl = {V4L2_CID_PAN_RESET};
    HANDLE_EINTR(ioctl(fd.get(), VIDIOC_S_CTRL, &v4l2_ctrl));
  }

  if (params->config.tilt) {
    struct v4l2_control v4l2_ctrl = {V4L2_CID_TILT_RESET};
    HANDLE_EINTR(ioctl(fd.get(), VIDIOC_S_CTRL, &v4l2_ctrl));
  }

  if (params->config.zoom) {
    const int kDefaultZoom = 100;
    SetWebcamParameter(fd.get(), V4L2_CID_ZOOM_ABSOLUTE, kDefaultZoom);
  }

  return true;
}

}  // namespace extensions
