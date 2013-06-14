// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/activity_log/dom_actions.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::BlockedChromeActivityDetail;

const char* DOMAction::kTableName = "activitylog_urls";
const char* DOMAction::kTableContentFields[] =
    {"url_action_type", "url_tld", "url_path", "url_title", "api_call",
     "args", "extra"};
const char* DOMAction::kTableFieldTypes[] =
    {"INTEGER", "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR",
     "LONGVARCHAR", "LONGVARCHAR"};

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

DOMAction::DOMAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
             base::Time::FromInternalValue(s.ColumnInt64(1)),
             ExtensionActivity::ACTIVITY_TYPE_DOM),
      verb_(static_cast<DomActionType::Type>(s.ColumnInt(2))),
      url_(GURL(s.ColumnString(3)+ s.ColumnString(4))),
      url_title_(s.ColumnString16(5)),
      api_call_(s.ColumnString(6)),
      args_(s.ColumnString(7)),
      extra_(s.ColumnString(8)) { }

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

// static
bool DOMAction::InitializeTable(sql::Connection* db) {
  // The original table schema was different than the existing one.
  // Sqlite doesn't let you delete or modify existing columns, so we drop it.
  // The old version can be identified because it had a field named
  // tech_message. Any data loss incurred here doesn't matter since these
  // fields existed before we started using the AL for anything.
  if (db->DoesColumnExist(kTableName, "tech_message")) {
    std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
    if (!db->Execute(drop_table.c_str()))
      return false;
  }
  // The url field is now broken into two parts - url_tld and url_path.
  // ulr_tld contains the scheme, host, and port of the url and
  // url_path contains the path.
  if (db->DoesColumnExist(kTableName, "url")) {
    std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
    if (!db->Execute(drop_table.c_str()))
      return false;
  }
  // We also now use INTEGER instead of VARCHAR for url_action_type.
  if (db->DoesColumnExist(kTableName, "url_action_type")) {
    std::string select = base::StringPrintf(
        "SELECT url_action_type FROM %s ORDER BY rowid LIMIT 1", kTableName);
    sql::Statement statement(db->GetUniqueStatement(select.c_str()));
    if (statement.DeclaredColumnType(0) != sql::COLUMN_TYPE_INTEGER) {
      std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
      if (!db->Execute(drop_table.c_str()))
        return false;
    }
  }
  // Now initialize the table.
  bool initialized = InitializeTableInternal(db,
                                             kTableName,
                                             kTableContentFields,
                                             kTableFieldTypes,
                                             arraysize(kTableContentFields));
  return initialized;
}

bool DOMAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName) +
      " (extension_id, time, url_action_type, url_tld, url_path, url_title,"
      "  api_call, args, extra) VALUES (?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  std::string url_tld;
  std::string url_path;
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindInt(2, static_cast<int>(verb_));
  url_tld = url_.GetOrigin().spec();
  // delete the extra "/"
  if ((url_tld.size() > 0) && (url_tld[url_tld.size()-1] == '/'))
    url_tld.erase(url_tld.size()-1);
  statement.BindString(3, url_tld);
  // If running in activity testing mode, store the parameters as well.
  if ((CommandLine::ForCurrentProcess()->HasSwitch(
       switches::kEnableExtensionActivityLogTesting)) && (url_.has_query()))
    url_path = url_.path()+"?"+url_.query();
  else
    url_path = url_.path();
  statement.BindString(4, url_path);
  statement.BindString16(5, url_title_);
  statement.BindString(6, api_call_);
  statement.BindString(7, args_);
  statement.BindString(8, extra_);
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
