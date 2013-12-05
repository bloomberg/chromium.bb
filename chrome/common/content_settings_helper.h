// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_HELPER_H_
#define CHROME_COMMON_CONTENT_SETTINGS_HELPER_H_

#include <string>

#include "base/strings/string16.h"

class GURL;

namespace content_settings_helper {

// Return simplified string representing origin. If origin is using http or
// the standard port, those parts are not included in the output.
std::string OriginToString(const GURL& origin);
base::string16 OriginToString16(const GURL& origin);

}  // namespace content_settings_helper

#endif  // CHROME_COMMON_CONTENT_SETTINGS_HELPER_H_
