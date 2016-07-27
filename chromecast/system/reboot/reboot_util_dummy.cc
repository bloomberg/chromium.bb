// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/reboot_util.h"

namespace chromecast {

// static
RebootShlib::RebootSource RebootUtil::GetLastRebootSource() {
    return RebootShlib::RebootSource::UNKNOWN;
}

// static
bool RebootUtil::SetLastRebootSource(
    RebootShlib::RebootSource /* reboot_source */) {
  return false;
}

}  // namespace chromecast
