// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/stream_noargs_ui_policy.h"

#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"

namespace constants = activity_log_constants;

namespace {

// We should log the arguments to these API calls.  Be careful when
// constructing this whitelist to not keep arguments that might compromise
// privacy by logging too much data to the activity log.
//
// TODO(mvrable): The contents of this whitelist should be reviewed and
// expanded as needed.
const char* kAlwaysLog[] = {"extension.connect", "extension.sendMessage",
                            "tabs.executeScript", "tabs.insertCSS"};

}  // namespace

namespace extensions {

StreamWithoutArgsUIPolicy::StreamWithoutArgsUIPolicy(Profile* profile)
    : FullStreamUIPolicy(profile) {
  for (size_t i = 0; i < arraysize(kAlwaysLog); i++) {
    arg_whitelist_api_.insert(kAlwaysLog[i]);
  }
}

StreamWithoutArgsUIPolicy::~StreamWithoutArgsUIPolicy() {}

scoped_refptr<Action> StreamWithoutArgsUIPolicy::ProcessArguments(
    scoped_refptr<Action> action) const {
  if (action->action_type() == Action::ACTION_DOM_ACCESS ||
      action->action_type() == Action::ACTION_DOM_EVENT ||
      arg_whitelist_api_.find(action->api_name()) != arg_whitelist_api_.end()) {
    // No stripping of arguments
  } else {
    // Do not modify the Action in-place, as there might be other users.
    action = action->Clone();
    action->set_args(scoped_ptr<ListValue>());

    // Strip details of the web request modifications (for privacy reasons).
    if (action->action_type() == Action::ACTION_WEB_REQUEST) {
      DictionaryValue* details = NULL;
      if (action->mutable_other()->GetDictionary(constants::kActionWebRequest,
                                                 &details)) {
        DictionaryValue::Iterator details_iterator(*details);
        while (!details_iterator.IsAtEnd()) {
          details->SetBoolean(details_iterator.key(), true);
          details_iterator.Advance();
        }
      }
    }
  }
  return action;
}

}  // namespace extensions
