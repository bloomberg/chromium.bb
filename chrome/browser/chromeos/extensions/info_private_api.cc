// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private_api.h"

#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;

namespace extensions {

namespace {

// Name of machine statistic property with HWID.
const char kHardwareClass[] = "hardware_class";

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

// Key which corresponds to the initial_locale property.
const char kPropertyInitialLocale[] = "initialLocale";

// Name of machine statistic property with board.
const char kPropertyReleaseBoard[] = "CHROMEOS_RELEASE_BOARD";

// Key which corresponds to the board property in JS.
const char kPropertyBoard[] = "board";

// Key which corresponds to the board property in JS.
const char kPropertyOwner[] = "isOwner";

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
    Value* value = NULL;
    if (GetValue(property_name, &value))
      result->Set(property_name, value);
  }
  SetResult(result.release());
  SendResponse(true);
  return true;
}

bool GetChromeosInfoFunction::GetValue(const std::string& property_name,
                                       Value** value) {
  if (property_name == kPropertyHWID) {
    std::string hwid;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(kHardwareClass, &hwid);
    *value = Value::CreateStringValue(hwid);
  } else if (property_name == kPropertyHomeProvider) {
    NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
    *value = Value::CreateStringValue(netlib->GetCellularHomeCarrierId());
  } else if (property_name == kPropertyInitialLocale) {
    *value = Value::CreateStringValue(
        chromeos::WizardController::GetInitialLocale());
  } else if (property_name == kPropertyBoard) {
    std::string board;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(kPropertyReleaseBoard, &board);
    *value = Value::CreateStringValue(board);
  } else if (property_name == kPropertyOwner) {
    *value = Value::CreateBooleanValue(
        chromeos::UserManager::Get()->IsCurrentUserOwner());
  } else {
    LOG(ERROR) << "Unknown property request: " << property_name;
    return false;
  }
  return true;
}

}  // namespace extensions
