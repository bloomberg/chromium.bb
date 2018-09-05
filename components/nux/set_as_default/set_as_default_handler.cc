// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nux/set_as_default/set_as_default_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

SetAsDefaultHandler::SetAsDefaultHandler() {}

SetAsDefaultHandler::~SetAsDefaultHandler() {}

void SetAsDefaultHandler::RegisterMessages() {}

void SetAsDefaultHandler::AddSources(content::WebUIDataSource* html_source) {
  // Localized strings.

  // Add required resources.
  html_source->AddResourcePath("nux_set_as_default.html",
                               IDR_NUX_SET_AS_DEFAULT_HTML);
  html_source->AddResourcePath("nux_set_as_default.js",
                               IDR_NUX_SET_AS_DEFAULT_JS);
  html_source->AddResourcePath("nux_set_as_default_proxy.html",
                               IDR_NUX_SET_AS_DEFAULT_PROXY_HTML);
  html_source->AddResourcePath("nux_set_as_default_proxy.js",
                               IDR_NUX_SET_AS_DEFAULT_PROXY_JS);

  // Add icons
}

}  // namespace nux