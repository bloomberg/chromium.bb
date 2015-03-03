// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui_generator/data_source_util.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/components_resources.h"

namespace webui_generator {

void SetUpDataSource(content::WebUIDataSource* data_source) {
  data_source->AddResourcePath("webui_generator/webui-view.html",
                               IDR_WUG_WEBUI_VIEW_HTML);
  data_source->AddResourcePath("webui_generator/webui-view.js",
                               IDR_WUG_WEBUI_VIEW_JS);
  data_source->AddResourcePath("webui_generator/context.js",
                               IDR_WUG_CONTEXT_JS);
  data_source->AddResourcePath("webui_generator/context.html",
                               IDR_WUG_CONTEXT_HTML);
}

}  // namespace webui_generator
