// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/physical_web_ui.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/webui/physical_web_ui_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

// Maximum value for the "selected position" histogram.
const int kSelectedPositionMaxValue = 50;

web::WebUIIOSDataSource* CreatePhysicalWebUIDataSource() {
  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(kChromeUIPhysicalWebHost);

  // Localized and data strings.
  html_source->AddLocalizedString(physical_web_ui::kTitle,
                                  IDS_PHYSICAL_WEB_UI_TITLE);

  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath(physical_web_ui::kPhysicalWebJS,
                               IDR_PHYSICAL_WEB_UI_JS);
  html_source->AddResourcePath(physical_web_ui::kPhysicalWebCSS,
                               IDR_PHYSICAL_WEB_UI_CSS);
  html_source->SetDefaultResource(IDR_PHYSICAL_WEB_UI_HTML);
  return html_source;
}

////////////////////////////////////////////////////////////////////////////////
//
// PhysicalWebDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome:physical-web page.
class PhysicalWebDOMHandler : public web::WebUIIOSMessageHandler {
 public:
  PhysicalWebDOMHandler() {}
  ~PhysicalWebDOMHandler() override {}

  void RegisterMessages() override;

  void HandleRequestNearbyURLs(const base::ListValue* args);
  void HandlePhysicalWebItemClicked(const base::ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebDOMHandler);
};

void PhysicalWebDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      physical_web_ui::kRequestNearbyUrls,
      base::Bind(&PhysicalWebDOMHandler::HandleRequestNearbyURLs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      physical_web_ui::kPhysicalWebItemClicked,
      base::Bind(&PhysicalWebDOMHandler::HandlePhysicalWebItemClicked,
                 base::Unretained(this)));
}

void PhysicalWebDOMHandler::HandleRequestNearbyURLs(
    const base::ListValue* args) {
  base::DictionaryValue results;

  std::unique_ptr<base::ListValue> metadata =
      GetApplicationContext()->GetPhysicalWebDataSource()->GetMetadata();

  // Add item indices. When an item is selected, the index of the selected item
  // is recorded in a UMA histogram.
  for (size_t i = 0; i < metadata->GetSize(); i++) {
    base::DictionaryValue* metadata_item = NULL;
    metadata->GetDictionary(i, &metadata_item);
    metadata_item->SetInteger(physical_web_ui::kIndex, i);
  }

  results.Set(physical_web_ui::kMetadata, metadata.release());

  // Pass the list of Physical Web URL metadata to the WebUI. A jstemplate will
  // create a list view with an item for each URL.
  web_ui()->CallJavascriptFunction(physical_web_ui::kReturnNearbyUrls, results);
}

void PhysicalWebDOMHandler::HandlePhysicalWebItemClicked(
    const base::ListValue* args) {
  int index;
  if (!args->GetInteger(0, &index)) {
    DLOG(ERROR) << "Invalid selection index";
    return;
  }

  // Record the index of the selected item. The index must be strictly less than
  // the maximum enumeration value.
  if (index > kSelectedPositionMaxValue) {
    index = kSelectedPositionMaxValue;
  }
  UMA_HISTOGRAM_EXACT_LINEAR("PhysicalWeb.WebUI.ListViewUrlPosition", index,
                             kSelectedPositionMaxValue);

  // Count the number of selections.
  base::RecordAction(
      base::UserMetricsAction("PhysicalWeb.WebUI.ListViewUrlSelected"));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// PhysicalWebUI
//
////////////////////////////////////////////////////////////////////////////////

PhysicalWebUI::PhysicalWebUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  PhysicalWebDOMHandler* handler = new PhysicalWebDOMHandler();
  web_ui->AddMessageHandler(handler);

  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreatePhysicalWebUIDataSource());

  base::RecordAction(base::UserMetricsAction("PhysicalWeb.WebUI.Open"));
}

PhysicalWebUI::~PhysicalWebUI() {}
