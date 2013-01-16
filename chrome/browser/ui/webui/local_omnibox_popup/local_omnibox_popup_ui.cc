// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_omnibox_popup/local_omnibox_popup_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "grit/browser_resources.h"


LocalOmniboxPopupUI::LocalOmniboxPopupUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  ChromeWebUIDataSource* source = new ChromeWebUIDataSource(
      chrome::kChromeUILocalOmniboxPopupHost);
  source->add_resource_path("local_omnibox_popup.css",
                            IDR_LOCAL_OMNIBOX_POPUP_CSS);
  source->add_resource_path("local_omnibox_popup.js",
                            IDR_LOCAL_OMNIBOX_POPUP_JS);
  source->add_resource_path("local_omnibox_popup.js",
                            IDR_LOCAL_OMNIBOX_POPUP_JS);

  source->add_resource_path("images/history_icon.png",
                            IDR_LOCAL_OMNIBOX_POPUP_IMAGES_HISTORY_ICON_PNG);
  source->add_resource_path("images/page_icon.png",
                            IDR_LOCAL_OMNIBOX_POPUP_IMAGES_PAGE_ICON_PNG);
  source->add_resource_path("images/search_icon.png",
                            IDR_LOCAL_OMNIBOX_POPUP_IMAGES_SEARCH_ICON_PNG);
  source->add_resource_path("images/2x/history_icon.png",
                            IDR_LOCAL_OMNIBOX_POPUP_IMAGES_2X_HISTORY_ICON_PNG);
  source->add_resource_path("images/2x/page_icon.png",
                            IDR_LOCAL_OMNIBOX_POPUP_IMAGES_2X_PAGE_ICON_PNG);
  source->add_resource_path("images/2x/search_icon.png",
                            IDR_LOCAL_OMNIBOX_POPUP_IMAGES_2X_SEARCH_ICON_PNG);

  source->set_default_resource(IDR_LOCAL_OMNIBOX_POPUP_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile, source);
}

LocalOmniboxPopupUI::~LocalOmniboxPopupUI() {
}
