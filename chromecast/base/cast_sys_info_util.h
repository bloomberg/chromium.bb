// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_SYS_INFO_UTIL_H_
#define CHROMECAST_BASE_CAST_SYS_INFO_UTIL_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace chromecast {

class CastSysInfo;

scoped_ptr<CastSysInfo> CreateSysInfo();

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_SYS_INFO_UTIL_H_
