// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/api_name_constants.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "sql/statement.h"

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

// Gets the URL for a given tab ID. Helper method for APIAction::LookupTabId.
std::string GetUrlForTabId(int tab_id, Profile* profile) {
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

}  // namespace

namespace extensions {

using api::activity_log_private::BlockedChromeActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ExtensionActivity;

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
      std::string url = GetUrlForTabId(tab_id, profile);
      if (url != std::string())
        args->Set(0, new base::StringValue(url));
    } else if ((api_call == "tabs.move" || api_call == "tabs.remove") &&
               args->GetList(0, &id_list)) {
      for (int i = 0; i < static_cast<int>(id_list->GetSize()); ++i) {
        if (id_list->GetInteger(i, &tab_id)) {
          std::string url = GetUrlForTabId(tab_id, profile);
          if (url != std::string())
            id_list->Set(i, new base::StringValue(url));
        } else {
          LOG(ERROR) << "The tab ID array is malformed at index " << i;
        }
      }
    }
  }
}

Action::Action(const std::string& extension_id,
               const base::Time& time,
               const ActionType action_type,
               const std::string& api_name)
    : extension_id_(extension_id),
      time_(time),
      action_type_(action_type),
      api_name_(api_name) {}

Action::~Action() {}

void Action::set_args(scoped_ptr<ListValue> args) {
  args_.reset(args.release());
}

ListValue* Action::mutable_args() {
  if (!args_.get()) {
    args_.reset(new ListValue());
  }
  return args_.get();
}

void Action::set_page_url(const GURL& page_url) {
  page_url_ = page_url;
}

void Action::set_arg_url(const GURL& arg_url) {
  arg_url_ = arg_url;
}

void Action::set_other(scoped_ptr<DictionaryValue> other) {
  other_.reset(other.release());
}

DictionaryValue* Action::mutable_other() {
  if (!other_.get()) {
    other_.reset(new DictionaryValue());
  }
  return other_.get();
}

bool Action::Record(sql::Connection* db) {
  std::string sql_str =
      "INSERT INTO " + std::string(FullStreamUIPolicy::kTableName) +
      " (extension_id, time, action_type, api_name, args, "
      "page_url, page_title, arg_url, other) VALUES (?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindInt(2, static_cast<int>(action_type()));
  statement.BindString(3, api_name());
  if (args()) {
    statement.BindString(4, Serialize(args()));
  } else {
    statement.BindNull(4);
  }
  if (other()) {
    statement.BindString(8, Serialize(other()));
  } else {
    statement.BindNull(8);
  }

  url_canon::Replacements<char> url_sanitizer;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogTesting)) {
    url_sanitizer.ClearQuery();
    url_sanitizer.ClearRef();
  }
  if (page_url().is_valid()) {
    statement.BindString(5, page_url().ReplaceComponents(url_sanitizer).spec());
  }
  statement.BindString(6, page_title());
  if (arg_url().is_valid()) {
    statement.BindString(7, arg_url().ReplaceComponents(url_sanitizer).spec());
  }

  if (!statement.Run()) {
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
    statement.Clear();
    return false;
  }
  return true;
}

scoped_ptr<ExtensionActivity> Action::ConvertToExtensionActivity() {
  scoped_ptr<ExtensionActivity> result(new ExtensionActivity);

  result->extension_id.reset(new std::string(extension_id()));
  result->time.reset(new double(time().ToJsTime()));

  switch (action_type()) {
    case ACTION_API_CALL:
    case ACTION_API_EVENT: {
      ChromeActivityDetail* details = new ChromeActivityDetail;
      if (action_type() == ACTION_API_CALL) {
        details->api_activity_type =
            ChromeActivityDetail::API_ACTIVITY_TYPE_CALL;
      } else {
        details->api_activity_type =
            ChromeActivityDetail::API_ACTIVITY_TYPE_EVENT_CALLBACK;
      }
      details->api_call.reset(new std::string(api_name()));
      details->args.reset(new std::string(Serialize(args())));
      details->extra.reset(new std::string(Serialize(other())));

      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_CHROME;
      result->chrome_activity_detail.reset(details);
      break;
    }

    case ACTION_API_BLOCKED: {
      BlockedChromeActivityDetail* details = new BlockedChromeActivityDetail;
      details->api_call.reset(new std::string(api_name()));
      details->args.reset(new std::string(Serialize(args())));
      details->extra.reset(new std::string(Serialize(other())));
      // TODO(mvrable): details->reason isn't filled in; fix this after
      // converting logging to using the types from
      // BlockedChromeActivityDetail::Reason.
      details->reason = BlockedChromeActivityDetail::REASON_NONE;

      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_BLOCKED_CHROME;
      result->blocked_chrome_activity_detail.reset(details);
      break;
    }

    case ACTION_DOM_EVENT:
    case ACTION_DOM_XHR:
    case ACTION_DOM_ACCESS:
    case ACTION_CONTENT_SCRIPT:
    case ACTION_WEB_REQUEST: {
      DomActivityDetail* details = new DomActivityDetail;

      if (action_type() == ACTION_WEB_REQUEST) {
        details->dom_activity_type =
            DomActivityDetail::DOM_ACTIVITY_TYPE_WEBREQUEST;
      } else if (action_type() == ACTION_CONTENT_SCRIPT) {
        details->dom_activity_type =
            DomActivityDetail::DOM_ACTIVITY_TYPE_INSERTED;
      } else {
        // TODO(mvrable): This ought to be filled in properly, but since the
        // API will change soon don't worry about it now.
        details->dom_activity_type =
            DomActivityDetail::DOM_ACTIVITY_TYPE_NONE;
      }
      details->api_call.reset(new std::string(api_name()));
      details->args.reset(new std::string(Serialize(args())));
      details->extra.reset(new std::string(Serialize(other())));
      details->url.reset(new std::string(page_url().spec()));
      if (!page_title().empty())
        details->url_title.reset(new std::string(page_title()));

      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_DOM;
      result->dom_activity_detail.reset(details);
      break;
    }

    default:
      LOG(WARNING) << "Bad activity log entry read from database (type="
                   << action_type_ << ")!";
  }

  return result.Pass();
}

std::string Action::PrintForDebug() {
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
  if (!page_title_.empty()) {
    StringValue title(page_title_);
    result += " PAGE_TITLE=" + Serialize(&title);
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
