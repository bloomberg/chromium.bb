// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/chrome_commands.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/stringprintf.h"
#include "chrome/test/automation/value_conversion_util.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_util.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace webdriver {

ExtensionsCommand::ExtensionsCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ExtensionsCommand::~ExtensionsCommand() {}

bool ExtensionsCommand::DoesGet() {
  return true;
}

bool ExtensionsCommand::DoesPost() {
  return true;
}

void ExtensionsCommand::ExecuteGet(Response* const response) {
  ListValue extensions_list;
  Error* error = session_->GetExtensionsInfo(&extensions_list);
  if (error) {
    response->SetError(error);
    return;
  }

  ListValue id_list;
  for (size_t i = 0; i < extensions_list.GetSize(); ++i) {
    DictionaryValue* extension_dict;
    if (!extensions_list.GetDictionary(i, &extension_dict)) {
      response->SetError(
          new Error(kUnknownError, "Invalid extension dictionary"));
      return;
    }
    bool is_component;
    if (!extension_dict->GetBoolean("is_component", &is_component)) {
      response->SetError(
          new Error(kUnknownError, "Missing or invalid 'is_component'"));
      return;
    }
    if (is_component)
      continue;

    std::string extension_id;
    if (!extension_dict->GetString("id", &extension_id)) {
      response->SetError(new Error(kUnknownError, "Missing or invalid 'id'"));
      return;
    }

    id_list.Append(Value::CreateStringValue(extension_id));
  }

  response->SetValue(id_list.DeepCopy());
}

void ExtensionsCommand::ExecutePost(Response* const response) {
  FilePath::StringType path_string;
  if (!GetStringParameter("path", &path_string)) {
    response->SetError(new Error(kBadRequest, "'path' missing or invalid"));
    return;
  }

  std::string extension_id;
  Error* error = session_->InstallExtension(
      FilePath(path_string), &extension_id);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(Value::CreateStringValue(extension_id));
}

ExtensionCommand::ExtensionCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ExtensionCommand::~ExtensionCommand() {}

bool ExtensionCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response))
    return false;

  // Path: "/session/$sessionId/chrome/extension/$id".
  extension_id_ = GetPathVariable(5);
  if (extension_id_.empty()) {
    response->SetError(new Error(kBadRequest, "Invalid extension ID"));
    return false;
  }
  return true;
}

bool ExtensionCommand::DoesGet() {
  return true;
}

bool ExtensionCommand::DoesPost() {
  return true;
}

bool ExtensionCommand::DoesDelete() {
  return true;
}

void ExtensionCommand::ExecuteGet(Response* const response) {
  ListValue extensions_list;
  Error* error = session_->GetExtensionsInfo(&extensions_list);
  if (error) {
    response->SetError(error);
    return;
  }

  bool found = false;
  DictionaryValue extension;
  for (size_t i = 0; i < extensions_list.GetSize(); ++i) {
    DictionaryValue* extension_dict;
    if (!extensions_list.GetDictionary(i, &extension_dict)) {
      response->SetError(
          new Error(kUnknownError, "Invalid extension dictionary"));
      return;
    }
    std::string id;
    if (!extension_dict->GetString("id", &id)) {
      response->SetError(
          new Error(kUnknownError, "Missing extension ID"));
      return;
    }
    if (id == extension_id_) {
      found = true;
      extension.Swap(extension_dict);
      break;
    }
  }

  if (!found) {
    response->SetError(
        new Error(kUnknownError, "Extension is not installed"));
    return;
  }

  bool is_enabled;
  if (!extension.GetBoolean("is_enabled", &is_enabled)) {
    response->SetError(
        new Error(kUnknownError, "Missing or invalid 'is_enabled'"));
    return;
  }
  bool has_page_action;
  if (!extension.GetBoolean("has_page_action", &has_page_action)) {
    response->SetError(
        new Error(kUnknownError, "Missing or invalid 'is_enabled'"));
    return;
  }

  bool is_visible = false;
  if (is_enabled && has_page_action) {
    // Only check page action visibility if we are enabled with a page action.
    // Otherwise Chrome will throw an error saying the extension does not have
    // a page action.
    error = session_->IsPageActionVisible(
        session_->current_target().view_id, extension_id_, &is_visible);
    if (error) {
      response->SetError(error);
      return;
    }
  }

  extension.SetBoolean("is_page_action_visible", is_visible);
  response->SetValue(extension.DeepCopy());
}

void ExtensionCommand::ExecutePost(Response* const response) {
  Error* error = NULL;
  if (HasParameter("enable")) {
    bool enable;
    if (!GetBooleanParameter("enable", &enable)) {
      response->SetError(new Error(kBadRequest, "'enable' must be a bool"));
      return;
    }
    error = session_->SetExtensionState(extension_id_, enable);
  } else if (HasParameter("click_button")) {
    std::string button;
    if (!GetStringParameter("click_button", &button)) {
      response->SetError(
          new Error(kBadRequest, "'click_button' must be a string"));
      return;
    }
    error = session_->ClickExtensionButton(extension_id_,
                                           button == "browser_action");
  } else {
    error = new Error(kBadRequest, "Missing action parameter");
  }

  if (error) {
    response->SetError(error);
    return;
  }
}

void ExtensionCommand::ExecuteDelete(Response* const response) {
  Error* error = session_->UninstallExtension(extension_id_);
  if (error) {
    response->SetError(error);
    return;
  }
}

ViewsCommand::ViewsCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ViewsCommand::~ViewsCommand() {}

bool ViewsCommand::DoesGet() {
  return true;
}

void ViewsCommand::ExecuteGet(Response* const response) {
  std::vector<WebViewInfo> views;
  Error* error = session_->GetViews(&views);
  if (error) {
    response->SetError(error);
    return;
  }
  ListValue* views_list = new ListValue();
  for (size_t i = 0; i < views.size(); ++i) {
    DictionaryValue* dict = new DictionaryValue();
    AutomationId id = views[i].view_id.GetId();
    dict->SetString("handle", WebViewIdToString(WebViewId::ForView(id)));
    dict->SetInteger("type", id.type());
    if (!views[i].extension_id.empty())
      dict->SetString("extension_id", views[i].extension_id);
    views_list->Append(dict);
  }
  response->SetValue(views_list);
}

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
HeapProfilerDumpCommand::HeapProfilerDumpCommand(
    const std::vector<std::string>& ps,
    const DictionaryValue* const parameters)
    : WebDriverCommand(ps, parameters) {}

HeapProfilerDumpCommand::~HeapProfilerDumpCommand() {}

bool HeapProfilerDumpCommand::DoesPost() {
  return true;
}

void HeapProfilerDumpCommand::ExecutePost(Response* const response) {
  std::string reason;
  if (!GetStringParameter("reason", &reason)) {
    response->SetError(new Error(kBadRequest, "'reason' missing or invalid"));
    return;
  }

  Error* error = session_->HeapProfilerDump(reason);
  if (error) {
    response->SetError(error);
    return;
  }
}
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

}  // namespace webdriver
