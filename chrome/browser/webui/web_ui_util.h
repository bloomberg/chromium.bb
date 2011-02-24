// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_WEB_UI_UTIL_H_
#define CHROME_BROWSER_WEBUI_WEB_UI_UTIL_H_
#pragma once

#include <string>

class SkBitmap;

namespace web_ui_util {

// Convenience routine to convert SkBitmap object to data url
// so that it can be used in WebUI.
std::string GetImageDataUrl(const SkBitmap& bitmap);

// Convenience routine to get data url that corresponds to given
// resource_id as an image. This function does not check if the
// resource for the |resource_id| is an image, therefore it is the
// caller's responsibility to make sure the resource is indeed an
// image. Returns empty string if a resource does not exist for given
// |resource_id|.
std::string GetImageDataUrlFromResource(int resource_id);

}  // namespace web_ui_util

#endif  // CHROME_BROWSER_WEBUI_WEB_UI_UTIL_H_
