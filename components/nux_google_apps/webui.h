// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NUX_GOOGLE_APPS_WEBUI_H_
#define COMPONENTS_NUX_GOOGLE_APPS_WEBUI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_data_source.h"

namespace nux_google_apps {

void AddSources(content::WebUIDataSource* html_source);

}  // namespace nux_google_apps

#endif  // COMPONENTS_NUX_GOOGLE_APPS_WEBUI_H_
