// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_info_private_api_chromeos.h"

#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/system_access.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;

namespace {

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

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
    chromeos::SystemAccess* system = chromeos::SystemAccess::GetInstance();
    system->GetMachineStatistic(kPropertyHWID, value);
  } else if (property_name == kPropertyHomeProvider) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
      (*value) = netlib->GetCellularHomeCarrierId();
    } else {
      LOG(ERROR) << "CrosLibrary can't be loaded.";
    }
  } else {
    LOG(ERROR) << "Unknown property request: " << property_name;
    return false;
  }
  return true;
}
