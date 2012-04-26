// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/camera_detector.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/system/udev_info_provider.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Sysfs directory containing V4L devices.
const char kV4LSubsystemDir[] = "/sys/class/video4linux/";
// Name of the udev property with V4L capabilities.
const char kV4LCapabilities[] = "ID_V4L_CAPABILITIES";
// Delimiter character for udev V4L capabilities.
const char kV4LCapabilitiesDelim = ':';
// V4L capability that denotes a capture-enabled device.
const char kV4LCaptureCapability[] = "capture";

}  // namespace

CameraDetector::CameraPresence CameraDetector::camera_presence_ =
    CameraDetector::kCameraPresenceUnknown;

bool CameraDetector::presence_check_in_progress_ = false;

void CameraDetector::StartPresenceCheck(const base::Closure& check_done) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Starting camera presence check";

  if (!presence_check_in_progress_) {
    presence_check_in_progress_ = true;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CameraDetector::CheckPresence),
        check_done);
  }
}

void CameraDetector::CheckPresence() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool present = false;

  system::UdevInfoProvider* udev_info = system::UdevInfoProvider::GetInstance();
  // We do a quick check using udev database because opening each /dev/videoX
  // device may trigger costly device initialization.
  using file_util::FileEnumerator;
  FileEnumerator file_enum(
      FilePath(kV4LSubsystemDir),
      false,  // Don't recurse.
      static_cast<FileEnumerator::FileType>(
          FileEnumerator::FILES | FileEnumerator::SHOW_SYM_LINKS));
  for (FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    std::string v4l_capabilities;
    if (udev_info->QueryDeviceProperty(path.value(), kV4LCapabilities,
                                       &v4l_capabilities)) {
      std::vector<std::string> caps;
      base::SplitString(v4l_capabilities, kV4LCapabilitiesDelim, &caps);
      if (find(caps.begin(), caps.end(), kV4LCaptureCapability) != caps.end()) {
        present = true;
        break;
      }
    }
  }

  camera_presence_ = present ? kCameraPresent : kCameraAbsent;
  presence_check_in_progress_ = false;

  DVLOG(1) << "Camera presence state: " << camera_presence_;
}

}  // namespace chromeos
