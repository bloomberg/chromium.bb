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

void StreamWithoutArgsUIPolicy::ProcessArguments(
    scoped_refptr<Action> action) const {
  if (action->action_type() == Action::ACTION_DOM_ACCESS ||
      action->action_type() == Action::ACTION_DOM_EVENT ||
      action->action_type() == Action::ACTION_DOM_XHR ||
      action->action_type() == Action::ACTION_WEB_REQUEST ||
      arg_whitelist_api_.find(action->api_name()) != arg_whitelist_api_.end()) {
    // No stripping of arguments
  } else {
    action->set_args(scoped_ptr<ListValue>());
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
