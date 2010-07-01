// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_rlz_module.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/common/extensions/extension.h"
#include "rlz/win/lib/lib_values.h"

namespace {

bool GetProductFromName(const std::string& product_name,
                        rlz_lib::Product* product) {
  bool success = true;
  switch (product_name[0]) {
    case 'B':
      *product = rlz_lib::FF_TOOLBAR;
      break;
    case 'C':
      *product = rlz_lib::CHROME;
      break;
    case 'D':
      *product = rlz_lib::DESKTOP;
      break;
    case 'K':
      *product = rlz_lib::QSB_WIN;
      break;
    case 'N':
      *product = rlz_lib::PINYIN_IME;
      break;
    case 'P':
      *product = rlz_lib::TOOLBAR_NOTIFIER;
      break;
    case 'T':
      *product = rlz_lib::IE_TOOLBAR;
      break;
    case 'U':
      *product = rlz_lib::PACK;
      break;
    case 'W':
      *product = rlz_lib::WEBAPPS;
      break;
    default:
      success = false;
      break;
  }

  return success;
}

bool GetEventFromName(const std::string& event_name,
                      rlz_lib::Event* event_id) {
  *event_id = rlz_lib::INVALID_EVENT;

  if (event_name == "install") {
    *event_id = rlz_lib::INSTALL;
  } else if (event_name == "set-to-google") {
    *event_id = rlz_lib::SET_TO_GOOGLE;
  } else if (event_name == "first-search") {
    *event_id = rlz_lib::FIRST_SEARCH;
  } else if (event_name == "activate") {
    *event_id = rlz_lib::ACTIVATE;
  }

  return *event_id != rlz_lib::INVALID_EVENT;
}

}  // namespace

bool RlzRecordProductEventFunction::RunImpl() {
  std::string product_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &product_name));
  rlz_lib::Product product;
  EXTENSION_FUNCTION_VALIDATE(GetProductFromName(product_name, &product));

  std::string ap_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &ap_name));
  rlz_lib::AccessPoint access_point;
  EXTENSION_FUNCTION_VALIDATE(rlz_lib::GetAccessPointFromName(ap_name.c_str(),
                                                              &access_point));

  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &event_name));
  rlz_lib::Event event_id;
  EXTENSION_FUNCTION_VALIDATE(GetEventFromName(event_name, &event_id));

  return RLZTracker::RecordProductEvent(product, access_point, event_id);
}

bool RlzGetAccessPointRlzFunction::RunImpl() {
  std::string ap_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &ap_name));
  rlz_lib::AccessPoint access_point;
  EXTENSION_FUNCTION_VALIDATE(rlz_lib::GetAccessPointFromName(ap_name.c_str(),
                                                              &access_point));

  char rlz[rlz_lib::kMaxRlzLength + 1];
  rlz_lib::GetAccessPointRlz(access_point, rlz, rlz_lib::kMaxRlzLength);
  result_.reset(Value::CreateStringValue(rlz));
  return true;
}

bool RlzClearProductStateFunction::RunImpl() {
  std::string product_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &product_name));
  rlz_lib::Product product;
  EXTENSION_FUNCTION_VALIDATE(GetProductFromName(product_name, &product));

  ListValue* access_points_list;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &access_points_list));
  if (access_points_list->GetSize() < 1) {
    EXTENSION_FUNCTION_ERROR("Access point array should not be empty.");
  }

  // Allocate an access point array to pass to ClearProductState().  The array
  // must be termindated with the value rlz_lib::NO_ACCESS_POINT, hence + 1
  // when allocating the array.
  scoped_array<rlz_lib::AccessPoint> access_points(
      new rlz_lib::AccessPoint[access_points_list->GetSize() + 1]);

  size_t i;
  for (i = 0; i < access_points_list->GetSize(); ++i) {
    std::string ap_name;
    EXTENSION_FUNCTION_VALIDATE(access_points_list->GetString(i, &ap_name));
    EXTENSION_FUNCTION_VALIDATE(rlz_lib::GetAccessPointFromName(
        ap_name.c_str(), &access_points[i]));
  }
  access_points[i] = rlz_lib::NO_ACCESS_POINT;

  rlz_lib::ClearProductState(product, access_points.get());
  return true;
}
