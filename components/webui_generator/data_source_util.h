// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WUG_DATA_SOURCE_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WUG_DATA_SOURCE_UTIL_H_

#include "components/webui_generator/export.h"

namespace content {
class WebUIDataSource;
}

namespace webui_generator {

// Adds common resources needed for WUG to |data_source|.
void WUG_EXPORT SetUpDataSource(content::WebUIDataSource* data_source);

}  // namespace webui_generator

#endif  // CHROME_BROWSER_UI_WEBUI_WUG_DATA_SOURCE_UTIL_H_
