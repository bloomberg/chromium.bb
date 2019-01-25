// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_data_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/common/api/lock_screen_data.h"

namespace extensions {

namespace {

std::string GetErrorString(lock_screen_data::OperationResult result) {
  switch (result) {
    case lock_screen_data::OperationResult::kSuccess:
    case lock_screen_data::OperationResult::kCount:
      NOTREACHED() << "Expected a failure code.";
      return "Unknown";
    case lock_screen_data::OperationResult::kFailed:
      return "Unknown";
    case lock_screen_data::OperationResult::kInvalidKey:
    case lock_screen_data::OperationResult::kWrongKey:
      return "Internal - encryption";
    case lock_screen_data::OperationResult::kAlreadyRegistered:
      return "Duplicate item";
    case lock_screen_data::OperationResult::kNotFound:
    case lock_screen_data::OperationResult::kUnknownExtension:
      return "Not found";
  }
  NOTREACHED() << "Unknown operation status";
  return "Unknown";
}

}  // namespace

LockScreenDataCreateFunction::LockScreenDataCreateFunction() {}

LockScreenDataCreateFunction::~LockScreenDataCreateFunction() {}

ExtensionFunction::ResponseAction LockScreenDataCreateFunction::Run() {
  lock_screen_data::LockScreenItemStorage* storage =
      lock_screen_data::LockScreenItemStorage::GetIfAllowed(browser_context());
  if (!storage) {
    LOG(ERROR) << "Attempt to create data item from context which cannot use "
               << "lock screen data item storage: " << source_context_type();
    return RespondNow(Error("Not available"));
  }

  storage->CreateItem(extension_id(),
                      base::Bind(&LockScreenDataCreateFunction::OnDone, this));
  return RespondLater();
}

void LockScreenDataCreateFunction::OnDone(
    lock_screen_data::OperationResult result,
    const lock_screen_data::DataItem* item) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.DataItemStorage.OperationResult.RegisterItem", result,
      lock_screen_data::OperationResult::kCount);

  if (result != lock_screen_data::OperationResult::kSuccess) {
    Respond(Error(GetErrorString(result)));
    return;
  }

  api::lock_screen_data::DataItemInfo item_info;
  item_info.id = item->id();
  Respond(
      ArgumentList(api::lock_screen_data::Create::Results::Create(item_info)));
}

LockScreenDataGetAllFunction::LockScreenDataGetAllFunction() {}

LockScreenDataGetAllFunction::~LockScreenDataGetAllFunction() {}

ExtensionFunction::ResponseAction LockScreenDataGetAllFunction::Run() {
  lock_screen_data::LockScreenItemStorage* storage =
      lock_screen_data::LockScreenItemStorage::GetIfAllowed(browser_context());
  if (!storage)
    return RespondNow(Error("Not available"));

  storage->GetAllForExtension(
      extension_id(), base::Bind(&LockScreenDataGetAllFunction::OnDone, this));
  return RespondLater();
}

void LockScreenDataGetAllFunction::OnDone(
    const std::vector<const lock_screen_data::DataItem*>& items) {
  std::vector<api::lock_screen_data::DataItemInfo> items_info;
  for (auto* const item : items) {
    if (!item)
      continue;
    api::lock_screen_data::DataItemInfo item_info;
    item_info.id = item->id();
    items_info.emplace_back(std::move(item_info));
  }

  Respond(
      ArgumentList(api::lock_screen_data::GetAll::Results::Create(items_info)));
}

LockScreenDataGetContentFunction::LockScreenDataGetContentFunction() {}

LockScreenDataGetContentFunction::~LockScreenDataGetContentFunction() {}

ExtensionFunction::ResponseAction LockScreenDataGetContentFunction::Run() {
  lock_screen_data::LockScreenItemStorage* storage =
      lock_screen_data::LockScreenItemStorage::GetIfAllowed(browser_context());
  if (!storage)
    return RespondNow(Error("Not available"));

  std::unique_ptr<api::lock_screen_data::GetContent::Params> params(
      api::lock_screen_data::GetContent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  storage->GetItemContent(
      extension_id(), params->id,
      base::Bind(&LockScreenDataGetContentFunction::OnDone, this));
  return RespondLater();
}

void LockScreenDataGetContentFunction::OnDone(
    lock_screen_data::OperationResult result,
    std::unique_ptr<std::vector<char>> data) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.DataItemStorage.OperationResult.ReadItem", result,
      lock_screen_data::OperationResult::kCount);

  if (result == lock_screen_data::OperationResult::kSuccess) {
    Respond(ArgumentList(api::lock_screen_data::GetContent::Results::Create(
        std::vector<uint8_t>(data->begin(), data->end()))));
    return;
  }
  Respond(Error(GetErrorString(result)));
}

LockScreenDataSetContentFunction::LockScreenDataSetContentFunction() {}

LockScreenDataSetContentFunction::~LockScreenDataSetContentFunction() {}

ExtensionFunction::ResponseAction LockScreenDataSetContentFunction::Run() {
  std::unique_ptr<api::lock_screen_data::SetContent::Params> params(
      api::lock_screen_data::SetContent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  lock_screen_data::LockScreenItemStorage* storage =
      lock_screen_data::LockScreenItemStorage::GetIfAllowed(browser_context());
  if (!storage)
    return RespondNow(Error("Not available"));

  storage->SetItemContent(
      extension_id(), params->id,
      std::vector<char>(params->data.begin(), params->data.end()),
      base::Bind(&LockScreenDataSetContentFunction::OnDone, this));
  return RespondLater();
}

void LockScreenDataSetContentFunction::OnDone(
    lock_screen_data::OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.DataItemStorage.OperationResult.WriteItem", result,
      lock_screen_data::OperationResult::kCount);

  if (result == lock_screen_data::OperationResult::kSuccess) {
    Respond(NoArguments());
    return;
  }
  Respond(Error(GetErrorString(result)));
}

LockScreenDataDeleteFunction::LockScreenDataDeleteFunction() {}

LockScreenDataDeleteFunction::~LockScreenDataDeleteFunction() {}

ExtensionFunction::ResponseAction LockScreenDataDeleteFunction::Run() {
  std::unique_ptr<api::lock_screen_data::Delete::Params> params(
      api::lock_screen_data::Delete::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  lock_screen_data::LockScreenItemStorage* storage =
      lock_screen_data::LockScreenItemStorage::GetIfAllowed(browser_context());
  if (!storage)
    return RespondNow(Error("Not available"));

  storage->DeleteItem(extension_id(), params->id,
                      base::Bind(&LockScreenDataDeleteFunction::OnDone, this));
  return RespondLater();
}

void LockScreenDataDeleteFunction::OnDone(
    lock_screen_data::OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.DataItemStorage.OperationResult.DeleteItem", result,
      lock_screen_data::OperationResult::kCount);

  if (result == lock_screen_data::OperationResult::kSuccess) {
    Respond(NoArguments());
    return;
  }
  Respond(Error(GetErrorString(result)));
}

}  // namespace extensions
