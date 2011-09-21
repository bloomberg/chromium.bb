// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/change_record.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/internal_api/base_node.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/protocol/proto_value_conversions.h"

namespace sync_api {

ChangeRecord::ChangeRecord()
    : id(kInvalidId), action(ACTION_ADD) {}

ChangeRecord::~ChangeRecord() {}

DictionaryValue* ChangeRecord::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  std::string action_str;
  switch (action) {
    case ACTION_ADD:
      action_str = "Add";
      break;
    case ACTION_DELETE:
      action_str = "Delete";
      break;
    case ACTION_UPDATE:
      action_str = "Update";
      break;
    default:
      NOTREACHED();
      action_str = "Unknown";
      break;
  }
  value->SetString("action", action_str);
  value->SetString("id", base::Int64ToString(id));
  if (action == ACTION_DELETE) {
    if (extra.get()) {
      value->Set("extra", extra->ToValue());
    }
    value->Set("specifics",
               browser_sync::EntitySpecificsToValue(specifics));
  }
  return value;
}

ExtraPasswordChangeRecordData::ExtraPasswordChangeRecordData() {}

ExtraPasswordChangeRecordData::ExtraPasswordChangeRecordData(
    const sync_pb::PasswordSpecificsData& data)
    : unencrypted_(data) {
}

ExtraPasswordChangeRecordData::~ExtraPasswordChangeRecordData() {}

DictionaryValue* ExtraPasswordChangeRecordData::ToValue() const {
  return browser_sync::PasswordSpecificsDataToValue(unencrypted_);
}

const sync_pb::PasswordSpecificsData&
    ExtraPasswordChangeRecordData::unencrypted() const {
  return unencrypted_;
}

}  // namespace sync_api

