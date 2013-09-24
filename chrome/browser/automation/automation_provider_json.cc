// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_json.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

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
    base::JSONWriter::Write(value, &json_string);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      message_, json_string, true);
  provider_->Send(message_);
  message_ = NULL;
}

void AutomationJSONReply::SendError(const std::string& error_message) {
  DCHECK(message_) << "Resending reply for JSON automation request";

  base::DictionaryValue dict;
  dict.SetString("error", error_message);
  std::string json;
  base::JSONWriter::Write(&dict, &json);

  AutomationMsg_SendJSONRequest::WriteReplyParams(message_, json, false);
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
  *browser = automation_util::GetBrowserAt(browser_index);
  if (!*browser) {
    *error = "Cannot locate browser from given index";
    return false;
  }
  return true;
}

bool GetTabFromJSONArgs(
    DictionaryValue* args,
    WebContents** tab,
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
  *tab = automation_util::GetWebContentsAt(browser_index, tab_index);
  if (!*tab) {
    *error = "Cannot locate tab from given indices";
    return false;
  }
  return true;
}

bool GetBrowserAndTabFromJSONArgs(
    DictionaryValue* args,
    Browser** browser,
    WebContents** tab,
    std::string* error) {
  return GetBrowserFromJSONArgs(args, browser, error) &&
         GetTabFromJSONArgs(args, tab, error);
}

bool GetRenderViewFromJSONArgs(
    DictionaryValue* args,
    Profile* profile,
    content::RenderViewHost** rvh,
    std::string* error) {
  WebContents* tab = NULL;
  if (!GetTabFromJSONArgs(args, &tab, error))
    return false;
  *rvh = tab->GetRenderViewHost();
  return true;
}

namespace {

bool GetExtensionFromJSONArgsHelper(
    base::DictionaryValue* args,
    const std::string& key,
    Profile* profile,
    bool include_disabled,
    const extensions::Extension** extension,
    std::string* error) {
  std::string id;
  if (!args->GetString(key, &id)) {
    *error = base::StringPrintf("Missing or invalid key: %s", key.c_str());
    return false;
  }
  ExtensionService* service = extensions::ExtensionSystem::Get(profile)->
      extension_service();
  if (!service) {
    *error = "No extensions service.";
    return false;
  }
  if (!service->GetInstalledExtension(id)) {
    // The extension ID does not correspond to any extension, whether crashed
    // or not.
    *error = base::StringPrintf("Extension %s is not installed.",
                                id.c_str());
    return false;
  }
  const extensions::Extension* installed_extension =
      service->GetExtensionById(id, include_disabled);
  if (!installed_extension) {
    *error = "Extension is disabled or has crashed.";
    return false;
  }
  *extension = installed_extension;
  return true;
}

}  // namespace

bool GetExtensionFromJSONArgs(
    base::DictionaryValue* args,
    const std::string& key,
    Profile* profile,
    const extensions::Extension** extension,
    std::string* error) {
  return GetExtensionFromJSONArgsHelper(
      args, key, profile, true /* include_disabled */, extension, error);
}

bool GetEnabledExtensionFromJSONArgs(
    base::DictionaryValue* args,
    const std::string& key,
    Profile* profile,
    const extensions::Extension** extension,
    std::string* error) {
  return GetExtensionFromJSONArgsHelper(
      args, key, profile, false /* include_disabled */, extension, error);
}
