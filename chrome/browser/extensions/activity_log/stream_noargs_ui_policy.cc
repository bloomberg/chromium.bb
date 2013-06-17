// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/extensions/activity_log/stream_noargs_ui_policy.h"

namespace extensions {

StreamWithoutArgsUIPolicy::StreamWithoutArgsUIPolicy(Profile* profile)
    : FullStreamUIPolicy(profile) {
  for (int i = 0; i < APIAction::kSizeAlwaysLog; i++) {
    arg_whitelist_api_.insert(APIAction::kAlwaysLog[i]);
  }
}

StreamWithoutArgsUIPolicy::~StreamWithoutArgsUIPolicy() {}

std::string StreamWithoutArgsUIPolicy::ProcessArguments(
    ActionType action_type,
    const std::string& name,
    const base::ListValue* args) const {
  if (action_type == ACTION_DOM ||
      arg_whitelist_api_.find(name) != arg_whitelist_api_.end()) {
    return FullStreamUIPolicy::ProcessArguments(action_type, name, args);
  } else {
    return std::string();
  }
}

void StreamWithoutArgsUIPolicy::ProcessWebRequestModifications(
    base::DictionaryValue& details,
    std::string& details_string) const {
  // Strip details of the web request modifications (for privacy reasons).
  DictionaryValue::Iterator details_iterator(details);
  while (!details_iterator.IsAtEnd()) {
    details.SetBoolean(details_iterator.key(), true);
    details_iterator.Advance();
  }
  JSONStringValueSerializer serializer(&details_string);
  serializer.SerializeAndOmitBinaryValues(details);
}

}  // namespace extensions
