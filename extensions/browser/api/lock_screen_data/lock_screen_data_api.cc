// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_data_api.h"

#include <memory>
#include <vector>

#include "extensions/common/api/lock_screen_data.h"

namespace extensions {

LockScreenDataCreateFunction::LockScreenDataCreateFunction() {}

LockScreenDataCreateFunction::~LockScreenDataCreateFunction() {}

ExtensionFunction::ResponseAction LockScreenDataCreateFunction::Run() {
  api::lock_screen_data::DataItemInfo item_info;
  item_info.id = "fake";
  return RespondNow(
      ArgumentList(api::lock_screen_data::Create::Results::Create(item_info)));
}

LockScreenDataGetAllFunction::LockScreenDataGetAllFunction() {}

LockScreenDataGetAllFunction::~LockScreenDataGetAllFunction() {}

ExtensionFunction::ResponseAction LockScreenDataGetAllFunction::Run() {
  std::vector<api::lock_screen_data::DataItemInfo> items_info;
  return RespondNow(
      ArgumentList(api::lock_screen_data::GetAll::Results::Create(items_info)));
}

LockScreenDataGetContentFunction::LockScreenDataGetContentFunction() {}

LockScreenDataGetContentFunction::~LockScreenDataGetContentFunction() {}

ExtensionFunction::ResponseAction LockScreenDataGetContentFunction::Run() {
  std::unique_ptr<api::lock_screen_data::GetContent::Params> params(
      api::lock_screen_data::GetContent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondNow(Error("Not found"));
}

LockScreenDataSetContentFunction::LockScreenDataSetContentFunction() {}

LockScreenDataSetContentFunction::~LockScreenDataSetContentFunction() {}

ExtensionFunction::ResponseAction LockScreenDataSetContentFunction::Run() {
  std::unique_ptr<api::lock_screen_data::SetContent::Params> params(
      api::lock_screen_data::SetContent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondNow(Error("Not found"));
}

LockScreenDataDeleteFunction::LockScreenDataDeleteFunction() {}

LockScreenDataDeleteFunction::~LockScreenDataDeleteFunction() {}

ExtensionFunction::ResponseAction LockScreenDataDeleteFunction::Run() {
  std::unique_ptr<api::lock_screen_data::Delete::Params> params(
      api::lock_screen_data::Delete::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  return RespondNow(Error("Not found."));
}

}  // namespace extensions
