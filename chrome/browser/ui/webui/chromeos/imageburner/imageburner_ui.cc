// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(crbug.com/427785): Remove this file, and related strings and
// constants after 2015-10-31.

#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

ImageBurnUI::ImageBurnUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIImageBurnerHost);
  source->SetDefaultResource(IDR_IMAGEBURNER_HTML);
  content::WebUIDataSource::Add(profile, source);
}
