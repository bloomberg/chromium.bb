// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/power_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/power/power_data_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

const char kStringsJsFile[] = "strings.js";
const char kRequestBatteryChargeDataCallback[] = "requestBatteryChargeData";
const char kOnRequestBatteryChargeDataFunction[] =
    "powerUI.showBatteryChargeData";

class PowerMessageHandler : public content::WebUIMessageHandler {
 public:
  PowerMessageHandler();
  virtual ~PowerMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void OnGetBatteryChargeData(const base::ListValue* value);
};

PowerMessageHandler::PowerMessageHandler() {
}

PowerMessageHandler::~PowerMessageHandler() {
}

void PowerMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestBatteryChargeDataCallback,
      base::Bind(&PowerMessageHandler::OnGetBatteryChargeData,
                 base::Unretained(this)));
}

void PowerMessageHandler::OnGetBatteryChargeData(const base::ListValue* value) {
  const std::deque<PowerDataCollector::PowerSupplySample>& power_supply =
      PowerDataCollector::Get()->power_supply_data();
  base::ListValue js_power_supply_data;
  for (size_t i = 0; i < power_supply.size(); ++i) {
    const PowerDataCollector::PowerSupplySample& sample = power_supply[i];
    scoped_ptr<base::DictionaryValue> element(new base::DictionaryValue);
    element->SetDouble("batteryPercent", sample.battery_percent);
    element->SetDouble("batteryDischargeRate", sample.battery_discharge_rate);
    element->SetBoolean("externalPower", sample.external_power);
    element->SetDouble("time", sample.time.ToJsTime());

    js_power_supply_data.Append(element.release());
  }

  const std::deque<PowerDataCollector::SystemResumedSample>& system_resumed =
      PowerDataCollector::Get()->system_resumed_data();
  base::ListValue js_system_resumed_data;
  for (size_t i = 0; i < system_resumed.size(); ++i) {
    const PowerDataCollector::SystemResumedSample& sample = system_resumed[i];
    scoped_ptr<base::DictionaryValue> element(new base::DictionaryValue);
    element->SetDouble("sleepDuration",
                       sample.sleep_duration.InMillisecondsF());
    element->SetDouble("time", sample.time.ToJsTime());

    js_system_resumed_data.Append(element.release());
  }

  web_ui()->CallJavascriptFunction(kOnRequestBatteryChargeDataFunction,
                                   js_power_supply_data,
                                   js_system_resumed_data);
}

}  // namespace

PowerUI::PowerUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new PowerMessageHandler());

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUIPowerHost);
  html->SetUseJsonJSFormatV2();

  html->AddLocalizedString("titleText", IDS_ABOUT_POWER_TITLE);
  html->AddLocalizedString("reloadButton", IDS_ABOUT_POWER_RELOAD_BUTTON);
  html->AddLocalizedString("batteryChargePercentageHeader",
                           IDS_ABOUT_POWER_BATTERY_CHARGE_PERCENTAGE_HEADER);
  html->AddLocalizedString("batteryDischargeRateHeader",
                           IDS_ABOUT_POWER_BATTERY_DISCHARGE_RATE_HEADER);
  html->AddLocalizedString("negativeDischargeRateInfo",
                           IDS_ABOUT_POWER_NEGATIVE_DISCHARGE_RATE_INFO);
  html->AddLocalizedString("notEnoughDataAvailableYet",
                           IDS_ABOUT_POWER_NOT_ENOUGH_DATA);
  html->AddLocalizedString("systemSuspended",
                           IDS_ABOUT_POWER_SYSTEM_SUSPENDED);
  html->SetJsonPath(kStringsJsFile);

  html->AddResourcePath("power.css", IDR_ABOUT_POWER_CSS);
  html->AddResourcePath("power.js", IDR_ABOUT_POWER_JS);
  html->SetDefaultResource(IDR_ABOUT_POWER_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html);
}

PowerUI::~PowerUI() {
}

}  // namespace chromeos
