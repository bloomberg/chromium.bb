// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/api_actions.h"
#include "chrome/browser/extensions/activity_log/api_name_constants.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Gets the URL for a given tab ID. Helper method for APIAction::LookupTabId.
std::string GetURLForTabId(const int tab_id, Profile* profile) {
  content::WebContents* contents = NULL;
  Browser* browser = NULL;
  bool found = ExtensionTabUtil::GetTabById(tab_id,
                                            profile,
                                            true,  // search incognito tabs too
                                            &browser,
                                            NULL,
                                            &contents,
                                            NULL);
  if (found) {
    // Check whether the profile the tab was found in is a normal or incognito
    // profile.
    if (!browser->profile()->IsOffTheRecord()) {
      GURL url = contents->GetURL();
      return std::string(url.spec());
    } else {
      return std::string(extensions::APIAction::kIncognitoUrl);
    }
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

std::string Serialize(const base::Value& value) {
  std::string value_as_text;
  JSONStringValueSerializer serializer(&value_as_text);
  serializer.SerializeAndOmitBinaryValues(value);
  return value_as_text;
}

}  // namespace

namespace extensions {

using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::BlockedChromeActivityDetail;

// We should log the arguments to these API calls, even if argument logging is
// disabled by default.
const char* APIAction::kAlwaysLog[] =
    {"extension.connect", "extension.sendMessage",
     "tabs.executeScript", "tabs.insertCSS" };
const int APIAction::kSizeAlwaysLog = arraysize(kAlwaysLog);

// A string used in place of the real URL when the URL is hidden because it is
// in an incognito window.  Extension activity logs mentioning kIncognitoUrl
// let the user know that an extension is manipulating incognito tabs without
// recording specific data about the pages.
const char* APIAction::kIncognitoUrl = "http://incognito/";

APIAction::APIAction(const std::string& extension_id,
                     const base::Time& time,
                     const Type type,
                     const std::string& api_call,
                     const std::string& args,
                     const base::ListValue& args_list,
                     const std::string& extra)
    : Action(extension_id, time, ExtensionActivity::ACTIVITY_TYPE_CHROME),
      type_(type),
      api_call_(api_call),
      args_(args),
      args_list_(args_list.DeepCopy()),
      extra_(extra) { }

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

bool APIAction::Record(sql::Connection* db) {
  std::string sql_str =
      "INSERT INTO " + std::string(FullStreamUIPolicy::kTableName) +
      " (extension_id, time, action_type, api_name, args) VALUES"
      " (?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  switch (type_) {
    case CALL:
      statement.BindInt(2, static_cast<int>(Action::ACTION_API_CALL));
      break;
    case EVENT_CALLBACK:
      statement.BindInt(2, static_cast<int>(Action::ACTION_API_EVENT));
      break;
    default:
      LOG(ERROR) << "Invalid action type: " << type_;
      return false;
  }
  statement.BindString(3, api_call_);
  statement.BindString(4, Serialize(*args_list_));
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

