// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NP_UTILS_H_
#define CHROME_FRAME_NP_UTILS_H_

#include <string>

#include "base/basictypes.h"
#include "chrome_frame/np_browser_functions.h"

namespace np_utils {

std::string GetLocation(NPP instance, NPObject* window);

}  // namespace np_utils

#endif  // CHROME_FRAME_NP_UTILS_H_
