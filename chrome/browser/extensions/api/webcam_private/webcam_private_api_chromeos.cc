// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webcam_private/webcam_private_api.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "chrome/common/extensions/api/webcam_private.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace {

base::ScopedFD OpenWebcam(const std::string& extension_id,
                          content::BrowserContext* browser_context,
                          const std::string& webcam_id) {
  // TODO(zork): Get device_id from content::MediaStreamManager.
  std::string device_id = "/dev/video0";

  return base::ScopedFD(HANDLE_EINTR(open(device_id.c_str(), 0)));
}

void SetWebcamParameter(int fd, int control_id, int value) {
  struct v4l2_control v4l2_ctrl = {control_id, value};
  HANDLE_EINTR(ioctl(fd, VIDIOC_S_CTRL, &v4l2_ctrl));
}

bool GetWebcamParameter(int fd, int control_id, int* value) {
  struct v4l2_control v4l2_ctrl = {control_id};

  if (HANDLE_EINTR(ioctl(fd, VIDIOC_G_CTRL, &v4l2_ctrl)))
    return false;

  *value = v4l2_ctrl.value;
  return true;
}

const char kUnknownWebcam[] = "Unknown webcam id";
}  // namespace

namespace extensions {

WebcamPrivateSetFunction::WebcamPrivateSetFunction() {
}

WebcamPrivateSetFunction::~WebcamPrivateSetFunction() {
}

bool WebcamPrivateSetFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::webcam_private::Set::Params> params(
      api::webcam_private::Set::Params::Create(*args_));
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

  if (params->config.tilt) {
    SetWebcamParameter(fd.get(), V4L2_CID_TILT_ABSOLUTE,
                       *(params->config.tilt));
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

bool WebcamPrivateGetFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::webcam_private::Get::Params> params(
      api::webcam_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ScopedFD fd =
      OpenWebcam(extension_id(), browser_context(), params->webcam_id);
  if (!fd.is_valid()) {
    SetError(kUnknownWebcam);
    return false;
  }

  api::webcam_private::WebcamConfiguration result;

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

bool WebcamPrivateResetFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::webcam_private::Reset::Params> params(
      api::webcam_private::Reset::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ScopedFD fd =
      OpenWebcam(extension_id(), browser_context(), params->webcam_id);
  if (!fd.is_valid()) {
    SetError(kUnknownWebcam);
    return false;
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
