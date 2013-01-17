// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/url_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char* UrlAction::kTableName = "activitylog_urls";
const char* UrlAction::kTableStructure = "("
    "extension_id LONGVARCHAR NOT NULL, "
    "time INTEGER NOT NULL, "
    "url_action_type LONGVARCHAR NOT NULL, "
    "url LONGVARCHAR NOT NULL, "
    "url_title LONGVARCHAR, "
    "tech_message LONGVARCHAR NOT NULL, "
    "extra LONGCHAR VAR NOT NULL)";

UrlAction::UrlAction(const std::string& extension_id,
                     const base::Time& time,
                     const UrlActionType verb,
                     const GURL& url,
                     const string16& url_title,
                     const std::string& tech_message,
                     const std::string& extra)
    : extension_id_(extension_id),
      time_(time),
      verb_(verb),
      url_(url),
      url_title_(url_title),
      technical_message_(tech_message),
      extra_(extra) { }

UrlAction::~UrlAction() {
}

void UrlAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName) +
      " (extension_id, time, url_action_type, url, url_title, tech_message,"
      "  extra) VALUES (?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id_);
  statement.BindInt64(1, time_.ToInternalValue());
  statement.BindString(2, VerbAsString());
  statement.BindString(3, history::URLDatabase::GURLToDatabaseURL(url_));
  statement.BindString16(4, url_title_);
  statement.BindString(5, technical_message_);
  statement.BindString(6, extra_);
  if (!statement.Run())
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
}

std::string UrlAction::PrettyPrintFori18n() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return PrettyPrintForDebug();
}

std::string UrlAction::PrettyPrintForDebug() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return "Injected scripts (" + technical_message_ + ") onto "
      + std::string(url_.spec());
}

std::string UrlAction::VerbAsString() const {
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

UrlAction::UrlActionType UrlAction::StringAsUrlActionType(
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

