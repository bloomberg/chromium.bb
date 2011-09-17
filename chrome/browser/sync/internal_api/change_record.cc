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

DictionaryValue* ChangeRecord::ToValue(const BaseTransaction* trans) const {
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
  Value* node_value = NULL;
  if (action == ACTION_DELETE) {
    DictionaryValue* node_dict = new DictionaryValue();
    node_dict->SetString("id", base::Int64ToString(id));
    node_dict->Set("specifics",
                    browser_sync::EntitySpecificsToValue(specifics));
    if (extra.get()) {
      node_dict->Set("extra", extra->ToValue());
    }
    node_value = node_dict;
  } else {
    ReadNode node(trans);
    if (node.InitByIdLookup(id)) {
      node_value = node.GetDetailsAsValue();
    }
  }
  if (!node_value) {
    NOTREACHED();
    node_value = Value::CreateNullValue();
  }
  value->Set("node", node_value);
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

