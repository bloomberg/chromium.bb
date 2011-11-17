// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_info_private_api_chromeos.h"

#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;

namespace {

// Name of machine statistic property with HWID.
const char kHardwareClass[] = "hardware_class";

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

// Key which corresponds to the initial_locale property.
const char kPropertyInitialLocale[] = "initialLocale";

}  // namespace

GetChromeosInfoFunction::GetChromeosInfoFunction() {
}

GetChromeosInfoFunction::~GetChromeosInfoFunction() {
}

bool GetChromeosInfoFunction::RunImpl() {
  ListValue* list = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &list));
  scoped_ptr<DictionaryValue> result(new DictionaryValue());
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string property_name;
    EXTENSION_FUNCTION_VALIDATE(list->GetString(i, &property_name));
    std::string value;
    if (GetValue(property_name, &value))
      result->Set(property_name, Value::CreateStringValue(value));
  }
  result_.reset(result.release());
  SendResponse(true);
  return true;
}

bool GetChromeosInfoFunction::GetValue(const std::string& property_name,
                                       std::string* value) {
  value->clear();
  if (property_name == kPropertyHWID) {
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(kHardwareClass, value);
  } else if (property_name == kPropertyHomeProvider) {
    NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
    (*value) = netlib->GetCellularHomeCarrierId();
  } else if (property_name == kPropertyInitialLocale) {
    *value = chromeos::WizardController::GetInitialLocale();
  } else {
    LOG(ERROR) << "Unknown property request: " << property_name;
    return false;
  }
  return true;
}
