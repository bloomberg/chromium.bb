// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/activity_log/dom_actions.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::BlockedChromeActivityDetail;

DOMAction::DOMAction(const std::string& extension_id,
                     const base::Time& time,
                     const DomActionType::Type verb,
                     const GURL& url,
                     const string16& url_title,
                     const std::string& api_call,
                     const std::string& args,
                     const std::string& extra)
    : Action(extension_id, time, ExtensionActivity::ACTIVITY_TYPE_DOM),
      verb_(verb),
      url_(url),
      url_title_(url_title),
      api_call_(api_call),
      args_(args),
      extra_(extra) { }

DOMAction::~DOMAction() {
}

scoped_ptr<ExtensionActivity> DOMAction::ConvertToExtensionActivity() {
  scoped_ptr<ExtensionActivity> formatted_activity;
  formatted_activity.reset(new ExtensionActivity);
  formatted_activity->extension_id.reset(
      new std::string(extension_id()));
  formatted_activity->activity_type = activity_type();
  formatted_activity->time.reset(new double(time().ToJsTime()));
  DomActivityDetail* details = new DomActivityDetail;
  details->dom_activity_type = DomActivityDetail::ParseDomActivityType(
      VerbAsString());
  details->url.reset(new std::string(url_.spec()));
  details->url_title.reset(new std::string(base::UTF16ToUTF8(url_title_)));
  details->api_call.reset(new std::string(api_call_));
  details->args.reset(new std::string(args_));
  details->extra.reset(new std::string(extra_));
  formatted_activity->dom_activity_detail.reset(details);
  return formatted_activity.Pass();
}

bool DOMAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " +
                        std::string(FullStreamUIPolicy::kTableName) +
                        " (extension_id, time, action_type, api_name, args, "
                        "page_url, arg_url, other) VALUES (?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  if (verb_ == DomActionType::INSERTED)
    statement.BindInt(2, static_cast<int>(Action::ACTION_CONTENT_SCRIPT));
  else
    statement.BindInt(2, static_cast<int>(Action::ACTION_DOM_ACCESS));
  statement.BindString(3, api_call_);

  ListValue args_list;
  args_list.AppendString(args_);
  std::string args_as_text;
  JSONStringValueSerializer serializer(&args_as_text);
  serializer.SerializeAndOmitBinaryValues(args_list);
  statement.BindString(4, args_as_text);

  // If running in activity testing mode, store the URL parameters as well.
  GURL database_url;
  if ((CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogTesting))) {
    database_url = url_;
  } else {
    url_canon::Replacements<char> sanitize;
    sanitize.ClearQuery();
    sanitize.ClearRef();
    database_url = url_.ReplaceComponents(sanitize);
  }
  statement.BindString(5, database_url.spec());

  if (verb_ == DomActionType::INSERTED)
    statement.BindString(6, args_);
  else
    statement.BindNull(6);

  DictionaryValue other;
  other.SetString("extra", extra_);
  other.SetString("page_title", url_title_);
  other.SetInteger("dom_verb", static_cast<int>(verb_));
  std::string other_string;
  JSONStringValueSerializer other_serializer(&other_string);
  other_serializer.SerializeAndOmitBinaryValues(other);
  statement.BindString(7, other_string);

  if (!statement.Run()) {
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
    statement.Clear();
    return false;
  }
  return true;
}

std::string DOMAction::PrintForDebug() {
  if (verb_ == DomActionType::INSERTED)
    return "Injected scripts (" + args_ + ") onto "
        + std::string(url_.spec()) + (extra_.empty() ? extra_ : " " + extra_);
  else
    return "DOM API CALL: " + api_call_ + ", ARGS: " + args_ + ", VERB: "
        + VerbAsString();
}

std::string DOMAction::VerbAsString() const {
  switch (verb_) {
    case DomActionType::GETTER:
      return "getter";
    case DomActionType::SETTER:
      return "setter";
    case DomActionType::METHOD:
      return "method";
    case DomActionType::INSERTED:
      return "inserted";
    case DomActionType::XHR:
      return "xhr";
    case DomActionType::WEBREQUEST:
      return "webrequest";
    case DomActionType::MODIFIED:    // legacy
      return "modified";
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace extensions
