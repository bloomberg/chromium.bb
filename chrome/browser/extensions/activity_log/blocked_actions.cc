// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/blocked_actions.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::BlockedChromeActivityDetail;

BlockedAction::BlockedAction(const std::string& extension_id,
                             const base::Time& time,
                             const std::string& api_call,
                             const std::string& args,
                             const BlockedAction::Reason reason,
                             const std::string& extra)
    : Action(extension_id,
             time,
             ExtensionActivity::ACTIVITY_TYPE_BLOCKED_CHROME),
      api_call_(api_call),
      args_(args),
      reason_(reason),
      extra_(extra) { }

BlockedAction::~BlockedAction() {
}

scoped_ptr<ExtensionActivity> BlockedAction::ConvertToExtensionActivity() {
  scoped_ptr<ExtensionActivity> formatted_activity;
  formatted_activity.reset(new ExtensionActivity);
  formatted_activity->extension_id.reset(
      new std::string(extension_id()));
  formatted_activity->activity_type = activity_type();
  formatted_activity->time.reset(new double(time().ToJsTime()));
  BlockedChromeActivityDetail* details = new BlockedChromeActivityDetail;
  details->api_call.reset(new std::string(api_call_));
  details->args.reset(new std::string(args_));
  details->reason = BlockedChromeActivityDetail::ParseReason(
      ReasonAsString());
  details->extra.reset(new std::string(extra_));
  formatted_activity->blocked_chrome_activity_detail.reset(details);
  return formatted_activity.Pass();
}

bool BlockedAction::Record(sql::Connection* db) {
  std::string sql_str =
      "INSERT INTO " + std::string(FullStreamUIPolicy::kTableName) +
      " (extension_id, time, action_type, api_name, args, other) VALUES"
      " (?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindInt(2, static_cast<int>(Action::ACTION_API_BLOCKED));
  statement.BindString(3, api_call_);
  statement.BindString(4, args_);

  DictionaryValue other;
  other.SetInteger("reason", static_cast<int>(reason_));
  std::string other_string;
  JSONStringValueSerializer other_serializer(&other_string);
  other_serializer.SerializeAndOmitBinaryValues(other);
  statement.BindString(5, other_string);

  if (!statement.Run()) {
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
    statement.Clear();
    return false;
  }
  return true;
}

std::string BlockedAction::PrintForDebug() {
  return "ID: " + extension_id() + ", blocked action " + api_call_ +
      ", reason: " + ReasonAsString();
}

std::string BlockedAction::ReasonAsString() const {
  if (reason_ == ACCESS_DENIED)
    return std::string("access_denied");
  else if (reason_ == QUOTA_EXCEEDED)
    return std::string("quota_exceeded");
  else
    return std::string("unknown_reason_type");  // To avoid Win header name.
}

}  // namespace extensions

