// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"

namespace {

std::string Serialize(const base::Value* value) {
  std::string value_as_text;
  if (!value) {
    value_as_text = "null";
  } else {
    JSONStringValueSerializer serializer(&value_as_text);
    serializer.SerializeAndOmitBinaryValues(*value);
  }
  return value_as_text;
}

}  // namespace

namespace extensions {

using api::activity_log_private::ExtensionActivity;

Action::Action(const std::string& extension_id,
               const base::Time& time,
               ExtensionActivity::ActivityType activity_type)
    : extension_id_(extension_id),
      time_(time),
      activity_type_(activity_type) {}

WatchdogAction::WatchdogAction(const std::string& extension_id,
                               const base::Time& time,
                               const ActionType action_type,
                               const std::string& api_name,
                               scoped_ptr<ListValue> args,
                               const GURL& page_url,
                               const GURL& arg_url,
                               scoped_ptr<DictionaryValue> other)
    : Action(extension_id, time, ExtensionActivity::ACTIVITY_TYPE_CHROME),
      action_type_(action_type),
      api_name_(api_name),
      args_(args.Pass()),
      page_url_(page_url),
      arg_url_(arg_url),
      other_(other.Pass()) {}

WatchdogAction::~WatchdogAction() {}

bool WatchdogAction::Record(sql::Connection* db) {
  // This methods isn't used and will go away entirely soon once database
  // writing moves to the policy objects.
  NOTREACHED();
  return true;
}

scoped_ptr<api::activity_log_private::ExtensionActivity>
WatchdogAction::ConvertToExtensionActivity() {
  scoped_ptr<api::activity_log_private::ExtensionActivity> result;
  return result.Pass();
}

std::string WatchdogAction::PrintForDebug() {
  std::string result = "ID=" + extension_id() + " CATEGORY=";
  switch (action_type_) {
    case ACTION_API_CALL:
      result += "api_call";
      break;
    case ACTION_API_EVENT:
      result += "api_event_callback";
      break;
    case ACTION_WEB_REQUEST:
      result += "webrequest";
      break;
    case ACTION_CONTENT_SCRIPT:
      result += "content_script";
      break;
    case ACTION_API_BLOCKED:
      result += "api_blocked";
      break;
    case ACTION_DOM_EVENT:
      result += "dom_event";
      break;
    case ACTION_DOM_XHR:
      result += "dom_xhr";
      break;
    case ACTION_DOM_ACCESS:
      result += "dom_access";
      break;
    default:
      result += base::StringPrintf("type%d", static_cast<int>(action_type_));
  }

  result += " API=" + api_name_;
  if (args_.get()) {
    result += " ARGS=" + Serialize(args_.get());
  }
  if (page_url_.is_valid()) {
    result += " PAGE_URL=" + page_url_.spec();
  }
  if (arg_url_.is_valid()) {
    result += " ARG_URL=" + arg_url_.spec();
  }
  if (other_.get()) {
    result += " OTHER=" + Serialize(other_.get());
  }

  return result;
}

}  // namespace extensions
