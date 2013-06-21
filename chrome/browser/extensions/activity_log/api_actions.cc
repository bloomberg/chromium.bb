// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/api_actions.h"
#include "chrome/browser/extensions/activity_log/api_name_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace {

// Gets the URL for a given tab ID. Helper method for APIAction::LookupTabId.
std::string GetURLForTabId(const int tab_id, Profile* profile) {
  content::WebContents* contents = NULL;
  bool found = ExtensionTabUtil::GetTabById(tab_id,
                                            profile,
                                            false,  // no incognito URLs
                                            NULL,
                                            NULL,
                                            &contents,
                                            NULL);
  if (found) {
    GURL url = contents->GetURL();
    return std::string(url.spec());
  } else {
    return std::string();
  }
}

// Sets up the hashmap for mapping extension strings to "ints". The hashmap is
// only set up once because it's quite long; the value is then cached.
class APINameMap {
 public:
  APINameMap() {
    SetupMap();
  }

  // activity_log_api_name_constants.h lists all known API calls as of 5/17.
  // This code maps each of those API calls (and events) to short strings
  // (integers converted to strings). They're all strings because (1) sqlite
  // databases are all strings underneath anyway and (2) the Lookup function
  // will simply return the original api_call string if we don't have it in our
  // lookup table.
  void SetupMap() {
    for (size_t i = 0;
         i < arraysize(activity_log_api_name_constants::kNames);
         ++i) {
      std::string name =
          std::string(activity_log_api_name_constants::kNames[i]);
      std::string num = base::IntToString(i);
      names_to_nums_[name] = num;
      nums_to_names_[num] = name;
    }
  }

  static APINameMap* GetInstance() {
    return Singleton<APINameMap>::get();
  }

  // This matches an api call to a number, if it's in the lookup table. If not,
  // it returns the original api call.
  const std::string& ApiToShortname(const std::string& api_call) {
    std::map<std::string, std::string>::iterator it =
        names_to_nums_.find(api_call);
    if (it == names_to_nums_.end())
      return api_call;
    else
      return it->second;
  }

  // This matches a number to an API call -- it's the opposite of
  // ApiToShortname.
  const std::string& ShortnameToApi(const std::string& shortname) {
    std::map<std::string, std::string>::iterator it =
        nums_to_names_.find(shortname);
    if (it == nums_to_names_.end())
      return shortname;
    else
      return it->second;
  }

 private:
  std::map<std::string, std::string> names_to_nums_;  // <name, number label>
  std::map<std::string, std::string> nums_to_names_;  // <number label, name>
};

}  // namespace

namespace extensions {

using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::BlockedChromeActivityDetail;

const char* APIAction::kTableName = "activitylog_apis";
const char* APIAction::kTableContentFields[] =
    {"api_type", "api_call", "args", "extra"};
const char* APIAction::kTableFieldTypes[] =
    {"INTEGER", "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR"};

// We should log the arguments to these API calls, even if argument logging is
// disabled by default.
const char* APIAction::kAlwaysLog[] =
    {"extension.connect", "extension.sendMessage",
     "tabs.executeScript", "tabs.insertCSS" };
const int APIAction::kSizeAlwaysLog = arraysize(kAlwaysLog);

APIAction::APIAction(const std::string& extension_id,
                     const base::Time& time,
                     const Type type,
                     const std::string& api_call,
                     const std::string& args,
                     const std::string& extra)
    : Action(extension_id, time, ExtensionActivity::ACTIVITY_TYPE_CHROME),
      type_(type),
      api_call_(api_call),
      args_(args),
      extra_(extra) { }

APIAction::APIAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
             base::Time::FromInternalValue(s.ColumnInt64(1)),
             ExtensionActivity::ACTIVITY_TYPE_CHROME),
      type_(static_cast<Type>(s.ColumnInt(2))),
      api_call_(APINameMap::GetInstance()->ShortnameToApi(s.ColumnString(3))),
      args_(s.ColumnString(4)),
      extra_(s.ColumnString(5)) { }

APIAction::~APIAction() {
}

scoped_ptr<ExtensionActivity> APIAction::ConvertToExtensionActivity() {
  scoped_ptr<ExtensionActivity> formatted_activity;
  formatted_activity.reset(new ExtensionActivity);
  formatted_activity->extension_id.reset(
      new std::string(extension_id()));
  formatted_activity->activity_type = activity_type();
  formatted_activity->time.reset(new double(time().ToJsTime()));
  ChromeActivityDetail* details = new ChromeActivityDetail;
  details->api_activity_type = ChromeActivityDetail::ParseApiActivityType(
      TypeAsString());
  details->api_call.reset(new std::string(api_call_));
  details->args.reset(new std::string(args_));
  details->extra.reset(new std::string(extra_));
  formatted_activity->chrome_activity_detail.reset(details);
  return formatted_activity.Pass();
}

// static
bool APIAction::InitializeTable(sql::Connection* db) {
  // The original table schema was different than the existing one.
  // We no longer want the api_action_type or target_type columns.
  // Sqlite doesn't let you delete or modify existing columns, so we drop it.
  // Any data loss incurred here doesn't matter since these fields existed
  // before we started using the AL for anything.
  if (db->DoesColumnExist(kTableName, "api_action_type")) {
    std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
    if (!db->Execute(drop_table.c_str()))
      return false;
  }
  // We also now use INTEGER instead of VARCHAR for api_type.
  if (db->DoesColumnExist(kTableName, "api_type")) {
    std::string select = base::StringPrintf(
        "SELECT api_type FROM %s ORDER BY rowid LIMIT 1", kTableName);
    sql::Statement statement(db->GetUniqueStatement(select.c_str()));
    if (statement.DeclaredColumnType(0) != sql::COLUMN_TYPE_INTEGER) {
      std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
      if (!db->Execute(drop_table.c_str()))
        return false;
    }
  }
  // Now initialize the table.
  return InitializeTableInternal(db,
                                 kTableName,
                                 kTableContentFields,
                                 kTableFieldTypes,
                                 arraysize(kTableContentFields));
}

bool APIAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName)
      + " (extension_id, time, api_type, api_call, args, extra) VALUES"
      " (?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindInt(2, static_cast<int>(type_));
  statement.BindString(3, APINameMap::GetInstance()->ApiToShortname(api_call_));
  statement.BindString(4, args_);
  statement.BindString(5, extra_);
  if (!statement.Run()) {
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
    statement.Clear();
    return false;
  }
  return true;
}

// static
void APIAction::LookupTabId(const std::string& api_call,
                            base::ListValue* args,
                            Profile* profile) {
  if (api_call == "tabs.get" ||                 // api calls, ID as int
      api_call == "tabs.connect" ||
      api_call == "tabs.sendMessage" ||
      api_call == "tabs.duplicate" ||
      api_call == "tabs.update" ||
      api_call == "tabs.reload" ||
      api_call == "tabs.detectLanguage" ||
      api_call == "tabs.executeScript" ||
      api_call == "tabs.insertCSS" ||
      api_call == "tabs.move" ||                // api calls, IDs in array
      api_call == "tabs.remove" ||
      api_call == "tabs.onUpdated" ||           // events, ID as int
      api_call == "tabs.onMoved" ||
      api_call == "tabs.onDetached" ||
      api_call == "tabs.onAttached" ||
      api_call == "tabs.onRemoved" ||
      api_call == "tabs.onReplaced") {
    int tab_id;
    base::ListValue* id_list;
    if (args->GetInteger(0, &tab_id)) {
      std::string url = GetURLForTabId(tab_id, profile);
      if (url != std::string())
        args->Set(0, new base::StringValue(url));
    } else if ((api_call == "tabs.move" || api_call == "tabs.remove") &&
               args->GetList(0, &id_list)) {
      for (int i = 0; i < static_cast<int>(id_list->GetSize()); ++i) {
        if (id_list->GetInteger(i, &tab_id)) {
          std::string url = GetURLForTabId(tab_id, profile);
          if (url != std::string())
            id_list->Set(i, new base::StringValue(url));
        } else {
          LOG(ERROR) << "The tab ID array is malformed at index " << i;
        }
      }
    }
  }
}

std::string APIAction::PrintForDebug() {
  return "ID: " + extension_id() + ", CATEGORY: " + TypeAsString() +
      ", API: " + api_call_ + ", ARGS: " + args_;
}

std::string APIAction::TypeAsString() const {
  switch (type_) {
    case CALL:
      return "call";
    case EVENT_CALLBACK:
      return "event_callback";
    default:
      return "unknown_type";
  }
}

}  // namespace extensions

