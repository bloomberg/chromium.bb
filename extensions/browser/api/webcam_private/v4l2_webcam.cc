// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/v4l2_webcam.h"

#include <fcntl.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

#define V4L2_CID_PAN_SPEED (V4L2_CID_CAMERA_CLASS_BASE+32)
#define V4L2_CID_TILT_SPEED (V4L2_CID_CAMERA_CLASS_BASE+33)
#define V4L2_CID_PANTILT_CMD (V4L2_CID_CAMERA_CLASS_BASE+34)

// GUID of the Extension Unit for Logitech CC3300e motor control:
// {212de5ff-3080-2c4e-82d9-f587d00540bd}
#define UVC_GUID_LOGITECH_CC3000E_MOTORS          \
 {0x21, 0x2d, 0xe5, 0xff, 0x30, 0x80, 0x2c, 0x4e, \
  0x82, 0xd9, 0xf5, 0x87, 0xd0, 0x05, 0x40, 0xbd}

#define LOGITECH_MOTORCONTROL_PANTILT_CMD 2

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
}  // namespace

namespace extensions {

V4L2Webcam::V4L2Webcam(const std::string& device_id) : device_id_(device_id) {
}

V4L2Webcam::~V4L2Webcam() {
}

bool V4L2Webcam::Open() {
  fd_.reset(HANDLE_EINTR(open(device_id_.c_str(), 0)));
  return fd_.is_valid();
}

bool V4L2Webcam::EnsureLogitechCommandsMapped() {
  int res =
      HANDLE_EINTR(ioctl(fd_.get(), UVCIOC_CTRL_MAP, &kLogitechCmdMapping));
  // If mapping is successful or it's already mapped, this is a Logitech
  // camera.
  // NOTE: On success, occasionally EFAULT is returned.  On a real error,
  // ENOMEM, EPERM, EINVAL, or EOVERFLOW should be returned.
  return res >= 0 || errno == EEXIST || errno == EFAULT;
}

bool V4L2Webcam::SetWebcamParameter(int fd, uint32_t control_id, int value) {
  struct v4l2_control v4l2_ctrl = {control_id, value};
  int res = HANDLE_EINTR(ioctl(fd, VIDIOC_S_CTRL, &v4l2_ctrl)) >= 0;
  return res >= 0;
}

bool V4L2Webcam::GetWebcamParameter(int fd, uint32_t control_id, int* value) {
  struct v4l2_control v4l2_ctrl = {control_id};

  if (HANDLE_EINTR(ioctl(fd, VIDIOC_G_CTRL, &v4l2_ctrl)))
    return false;

  *value = v4l2_ctrl.value;
  return true;
}

void V4L2Webcam::Reset(bool pan, bool tilt, bool zoom) {
  if (pan || tilt) {
    if (EnsureLogitechCommandsMapped()) {
      SetWebcamParameter(fd_.get(), V4L2_CID_PANTILT_CMD,
                         kLogitechMenuIndexGoHome);
    } else {
      if (pan) {
        struct v4l2_control v4l2_ctrl = {V4L2_CID_PAN_RESET};
        HANDLE_EINTR(ioctl(fd_.get(), VIDIOC_S_CTRL, &v4l2_ctrl));
      }

      if (tilt) {
        struct v4l2_control v4l2_ctrl = {V4L2_CID_TILT_RESET};
        HANDLE_EINTR(ioctl(fd_.get(), VIDIOC_S_CTRL, &v4l2_ctrl));
      }
    }
  }

  if (zoom) {
    const int kDefaultZoom = 100;
    SetWebcamParameter(fd_.get(), V4L2_CID_ZOOM_ABSOLUTE, kDefaultZoom);
  }
}

bool V4L2Webcam::GetPan(int* value) {
  return GetWebcamParameter(fd_.get(), V4L2_CID_PAN_ABSOLUTE, value);
}

bool V4L2Webcam::GetTilt(int* value) {
  return GetWebcamParameter(fd_.get(), V4L2_CID_TILT_ABSOLUTE, value);
}

bool V4L2Webcam::GetZoom(int* value) {
  return GetWebcamParameter(fd_.get(), V4L2_CID_ZOOM_ABSOLUTE, value);
}

bool V4L2Webcam::SetPan(int value) {
  return SetWebcamParameter(fd_.get(), V4L2_CID_PAN_ABSOLUTE, value);
}

bool V4L2Webcam::SetTilt(int value) {
  return SetWebcamParameter(fd_.get(), V4L2_CID_TILT_ABSOLUTE, value);
}

bool V4L2Webcam::SetZoom(int value) {
  return SetWebcamParameter(fd_.get(), V4L2_CID_ZOOM_ABSOLUTE, value);
}

bool V4L2Webcam::SetPanDirection(PanDirection direction) {
  int direction_value = 0;
  switch (direction) {
    case PAN_STOP:
      direction_value = 0;
      break;

    case PAN_RIGHT:
      direction_value = 1;
      break;

    case PAN_LEFT:
      direction_value = -1;
      break;
  }
  return SetWebcamParameter(fd_.get(), V4L2_CID_PAN_SPEED, direction_value);
}

bool V4L2Webcam::SetTiltDirection(TiltDirection direction) {
  int direction_value = 0;
  switch (direction) {
    case TILT_STOP:
      direction_value = 0;
      break;

    case TILT_UP:
      direction_value = 1;
      break;

    case TILT_DOWN:
      direction_value = -1;
      break;
  }
  return SetWebcamParameter(fd_.get(), V4L2_CID_TILT_SPEED, direction_value);
}

}  // namespace extensions
