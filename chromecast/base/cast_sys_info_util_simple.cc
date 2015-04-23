// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_util.h"

#include "chromecast/base/cast_sys_info_dummy.h"

namespace chromecast {

// static
scoped_ptr<CastSysInfo> CreateSysInfo() {
  return make_scoped_ptr(new CastSysInfoDummy());
}

}  // namespace chromecast
