// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char* APIAction::kTableName = "activitylog_apis";
const char* APIAction::kTableContentFields[] =
    {"api_type", "api_action_type", "target_type", "api_call", "args", "extra"};

// We should log the arguments to these API calls, even if argument logging is
// disabled by default.
const char* APIAction::kAlwaysLog[] =
    {"extension.connect", "extension.sendMessage",
     "tabs.executeScript", "tabs.insertCSS" };
const int APIAction::kSizeAlwaysLog = arraysize(kAlwaysLog);

APIAction::APIAction(const std::string& extension_id,
                     const base::Time& time,
                     const Type type,
                     const Verb verb,
                     const Target target,
                     const std::string& api_call,
                     const std::string& args,
                     const std::string& extra)
    : Action(extension_id, time),
      type_(type),
      verb_(verb),
      target_(target),
      api_call_(api_call),
      args_(args),
      extra_(extra) { }

APIAction::APIAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
          base::Time::FromInternalValue(s.ColumnInt64(1))),
      type_(StringAsType(s.ColumnString(2))),
      verb_(StringAsVerb(s.ColumnString(3))),
      target_(StringAsTarget(s.ColumnString(4))),
      api_call_(s.ColumnString(5)),
      args_(s.ColumnString(6)),
      extra_(s.ColumnString(7)) { }

APIAction::~APIAction() {
}

// static
bool APIAction::InitializeTable(sql::Connection* db) {
  return InitializeTableInternal(db,
                                 kTableName,
                                 kTableContentFields,
                                 arraysize(kTableContentFields));
}

void APIAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName)
      + " (extension_id, time, api_type, api_action_type, target_type,"
      " api_call, args, extra) VALUES (?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindString(2, TypeAsString());
  statement.BindString(3, VerbAsString());
  statement.BindString(4, TargetAsString());
  statement.BindString(5, api_call_);
  statement.BindString(6, args_);
  statement.BindString(7, extra_);
  if (!statement.Run())
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
}

std::string APIAction::PrettyPrintFori18n() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return PrettyPrintForDebug();
}

std::string APIAction::PrettyPrintForDebug() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return "ID: " + extension_id() + ", CATEGORY: " + TypeAsString() +
      ", VERB: " + VerbAsString() + ", TARGET: " + TargetAsString() +
      ", API: " + api_call_ + ", ARGS: " + args_;
}

std::string APIAction::TypeAsString() const {
  switch (type_) {
    case CALL:
      return "CALL";
    case EVENT_CALLBACK:
      return "EVENT_CALLBACK";
    default:
      return "UNKNOWN_TYPE";
  }
}

std::string APIAction::VerbAsString() const {
  switch (verb_) {
    case READ:
      return "READ";
    case MODIFIED:
      return "MODIFIED";
    case DELETED:
      return "DELETED";
    case ADDED:
      return "ADDED";
    case ENABLED:
      return "ENABLED";
    case DISABLED:
      return "DISABLED";
    case CREATED:
      return "CREATED";
    default:
      return "UNKNOWN_VERB";
  }
}

std::string APIAction::TargetAsString() const {
  switch (target_) {
    case BOOKMARK:
      return "BOOKMARK";
    case TABS:
      return "TABS";
    case HISTORY:
      return "HISTORY";
    case COOKIES:
      return "COOKIES";
    case BROWSER_ACTION:
      return "BROWSER_ACTION";
    case NOTIFICATION:
      return "NOTIFICATION";
    case OMNIBOX:
      return "OMNIBOX";
    default:
      return "UNKNOWN_TARGET";
  }
}

APIAction::Type APIAction::StringAsType(
    const std::string& str) {
  if (str == "CALL") {
    return CALL;
  } else if (str == "EVENT_CALLBACK") {
    return EVENT_CALLBACK;
  } else {
    return UNKNOWN_TYPE;
  }
}

APIAction::Verb APIAction::StringAsVerb(
    const std::string& str) {
  if (str == "READ") {
    return READ;
  } else if (str == "MODIFIED") {
    return MODIFIED;
  } else if (str == "DELETED") {
    return DELETED;
  } else if (str == "ADDED") {
    return ADDED;
  } else if (str == "ENABLED") {
    return ENABLED;
  } else if (str == "DISABLED") {
    return DISABLED;
  } else if (str == "CREATED") {
    return CREATED;
  } else {
    return UNKNOWN_VERB;
  }
}

// The all-caps strings match the enum names.  The lowercase strings match the
// actual object names (e.g., cookies.remove(...);).
APIAction::Target APIAction::StringAsTarget(
    const std::string& str) {
  if (str == "BOOKMARK" || str == "bookmark") {
    return BOOKMARK;
  } else if (str == "TABS" || str == "tabs") {
    return TABS;
  } else if (str == "HISTORY" || str == "history") {
    return HISTORY;
  } else if (str == "COOKIES" || str == "cookies") {
    return COOKIES;
  } else if (str == "BROWSER_ACTION" || str == "browser_action") {
    return BROWSER_ACTION;
  } else if (str == "NOTIFICATION" || str == "notification") {
    return NOTIFICATION;
  } else if (str == "OMNIBOX" || str == "omnibox") {
    return OMNIBOX;
  } else {
    return UNKNOWN_TARGET;
  }
}

}  // namespace extensions
