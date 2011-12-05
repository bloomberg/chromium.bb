// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_json.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/common/automation_id.h"
#include "chrome/common/automation_messages.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

// Util for creating a JSON error return string (dict with key
// 'error' and error string value).  No need to quote input.
std::string JSONErrorString(const std::string& err) {
  std::string prefix = "{\"error\": \"";
  std::string no_quote_err;
  std::string suffix = "\"}";

  base::JsonDoubleQuote(err, false, &no_quote_err);
  return prefix + no_quote_err + suffix;
}

}  // namespace

AutomationJSONReply::AutomationJSONReply(AutomationProvider* provider,
                                         IPC::Message* reply_message)
  : provider_(provider),
    message_(reply_message) {
}

AutomationJSONReply::~AutomationJSONReply() {
  DCHECK(!message_) << "JSON automation request not replied!";
}

void AutomationJSONReply::SendSuccess(const Value* value) {
  DCHECK(message_) << "Resending reply for JSON automation request";
  std::string json_string = "{}";
  if (value)
    base::JSONWriter::Write(value, false, &json_string);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      message_, json_string, true);
  provider_->Send(message_);
  message_ = NULL;
}

void AutomationJSONReply::SendError(const std::string& error_message) {
  DCHECK(message_) << "Resending reply for JSON automation request";
  std::string json_string = JSONErrorString(error_message);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      message_, json_string, false);
  provider_->Send(message_);
  message_ = NULL;
}

bool GetBrowserFromJSONArgs(
    DictionaryValue* args,
    Browser** browser,
    std::string* error) {
  if (args->HasKey("auto_id")) {
    AutomationId id;
    if (!GetAutomationIdFromJSONArgs(args, "auto_id", &id, error))
      return false;
    TabContents* tab;
    if (!automation_util::GetTabForId(id, &tab)) {
      *error = "'auto_id' does not refer to an open tab";
      return false;
    }
    Browser* container = automation_util::GetBrowserForTab(tab);
    if (!container) {
      *error = "tab does not belong to an open browser";
      return false;
    }
    *browser = container;
  } else {
    int browser_index;
    if (!args->GetInteger("windex", &browser_index)) {
      *error = "'windex' missing or invalid";
      return false;
    }
    *browser = automation_util::GetBrowserAt(browser_index);
    if (!*browser) {
      *error = "Cannot locate browser from given index";
      return false;
    }
  }
  return true;
}

bool GetTabFromJSONArgs(
    DictionaryValue* args,
    TabContents** tab,
    std::string* error) {
  if (args->HasKey("auto_id")) {
    AutomationId id;
    if (!GetAutomationIdFromJSONArgs(args, "auto_id", &id, error))
      return false;
    if (!automation_util::GetTabForId(id, tab)) {
      *error = "'auto_id' does not refer to an open tab";
      return false;
    }
  } else {
    int browser_index, tab_index;
    if (!args->GetInteger("windex", &browser_index)) {
      *error = "'windex' missing or invalid";
      return false;
    }
    if (!args->GetInteger("tab_index", &tab_index)) {
      *error = "'tab_index' missing or invalid";
      return false;
    }
    *tab = automation_util::GetTabContentsAt(browser_index, tab_index);
    if (!*tab) {
      *error = "Cannot locate tab from given indices";
      return false;
    }
  }
  return true;
}

bool GetBrowserAndTabFromJSONArgs(
    DictionaryValue* args,
    Browser** browser,
    TabContents** tab,
    std::string* error) {
  return GetBrowserFromJSONArgs(args, browser, error) &&
         GetTabFromJSONArgs(args, tab, error);
}

bool GetAutomationIdFromJSONArgs(
    DictionaryValue* args,
    const std::string& key_name,
    AutomationId* id,
    std::string* error) {
  Value* id_value;
  if (!args->Get(key_name, &id_value)) {
    *error = base::StringPrintf("Missing parameter '%s'", key_name.c_str());
    return false;
  }
  return AutomationId::FromValue(id_value, id, error);
}

bool GetRenderViewFromJSONArgs(
    DictionaryValue* args,
    Profile* profile,
    RenderViewHost** rvh,
    std::string* error) {
  Value* id_value;
  if (args->Get("auto_id", &id_value)) {
    AutomationId id;
    if (!AutomationId::FromValue(id_value, &id, error))
      return false;
    if (!automation_util::GetRenderViewForId(id, profile, rvh)) {
      *error = "ID does not correspond to an open view";
      return false;
    }
  } else {
    // If the render view id is not specified, check for browser/tab indices.
    TabContents* tab = NULL;
    if (!GetTabFromJSONArgs(args, &tab, error))
      return false;
    *rvh = tab->render_view_host();
  }
  return true;
}
