// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/dom_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char* DOMAction::kTableName = "activitylog_urls";
const char* DOMAction::kTableContentFields[] =
    {"url_action_type", "url", "url_title", "api_call", "args", "extra"};

DOMAction::DOMAction(const std::string& extension_id,
                     const base::Time& time,
                     const DOMActionType verb,
                     const GURL& url,
                     const string16& url_title,
                     const std::string& api_call,
                     const std::string& args,
                     const std::string& extra)
    : Action(extension_id, time),
      verb_(verb),
      url_(url),
      url_title_(url_title),
      api_call_(api_call),
      args_(args),
      extra_(extra) { }

DOMAction::DOMAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
          base::Time::FromInternalValue(s.ColumnInt64(1))),
      verb_(StringAsDOMActionType(s.ColumnString(2))),
      url_(GURL(s.ColumnString(3))),
      url_title_(s.ColumnString16(4)),
      api_call_(s.ColumnString(5)),
      args_(s.ColumnString(6)),
      extra_(s.ColumnString(7)) { }

DOMAction::~DOMAction() {
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
  // Now initialize the table.
  bool initialized = InitializeTableInternal(db,
                                             kTableName,
                                             kTableContentFields,
                                             arraysize(kTableContentFields));
  return initialized;
}

void DOMAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName) +
      " (extension_id, time, url_action_type, url, url_title, api_call, args,"
      "  extra) VALUES (?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindString(2, VerbAsString());
  statement.BindString(3, history::URLDatabase::GURLToDatabaseURL(url_));
  statement.BindString16(4, url_title_);
  statement.BindString(5, api_call_);
  statement.BindString(6, args_);
  statement.BindString(7, extra_);
  if (!statement.Run())
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
}

std::string DOMAction::PrettyPrintFori18n() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return PrettyPrintForDebug();
}

std::string DOMAction::PrettyPrintForDebug() {
  // TODO(felt): implement this for real when the UI is redesigned.
  if (verb_ == INSERTED)
    return "Injected scripts (" + args_ + ") onto "
      + std::string(url_.spec());
  else
    return "DOM API CALL: " + api_call_ + ", ARGS: " + args_;
}

std::string DOMAction::VerbAsString() const {
  switch (verb_) {
    case MODIFIED:
      return "MODIFIED";
    case READ:
      return "READ";
    case INSERTED:
      return "INSERTED";
    case XHR:
      return "XHR";
    default:
      NOTREACHED();
      return NULL;
  }
}

DOMAction::DOMActionType DOMAction::StringAsDOMActionType(
    const std::string& str) {
  if (str == "MODIFIED") {
    return MODIFIED;
  } else if (str == "READ") {
    return READ;
  } else if (str == "INSERTED") {
    return INSERTED;
  } else if (str == "XHR") {
    return XHR;
  } else {
    NOTREACHED();
    return MODIFIED;  // this should never happen!
  }
}

}  // namespace extensions

