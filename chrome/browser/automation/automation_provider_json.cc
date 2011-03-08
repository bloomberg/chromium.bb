// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_json.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/automation_messages.h"

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

Browser* GetBrowserAt(int index) {
  if (index < 0 || index >= static_cast<int>(BrowserList::size()))
    return NULL;
  return *(BrowserList::begin() + index);
}

TabContents* GetTabContentsAt(int browser_index, int tab_index) {
  if (tab_index < 0)
    return NULL;
  Browser* browser = GetBrowserAt(browser_index);
  if (!browser || tab_index >= browser->tab_count())
    return NULL;
  return browser->GetTabContentsAt(tab_index);
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
  int browser_index;
  if (!args->GetInteger("windex", &browser_index)) {
    *error = "'windex' missing or invalid";
    return false;
  }
  *browser = GetBrowserAt(browser_index);
  if (!*browser) {
    *error = "Cannot locate browser from given index";
    return false;
  }
  return true;
}

bool GetTabFromJSONArgs(
    DictionaryValue* args,
    TabContents** tab,
    std::string* error) {
  int browser_index, tab_index;
  if (!args->GetInteger("windex", &browser_index)) {
    *error = "'windex' missing or invalid";
    return false;
  }
  if (!args->GetInteger("tab_index", &tab_index)) {
    *error = "'tab_index' missing or invalid";
    return false;
  }
  *tab = GetTabContentsAt(browser_index, tab_index);
  if (!*tab) {
    *error = "Cannot locate tab from given indices";
    return false;
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
