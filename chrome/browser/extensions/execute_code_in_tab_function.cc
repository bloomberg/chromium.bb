// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/execute_code_in_tab_function.h"

#include "base/callback.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/file_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/notification_service.h"

namespace keys = extension_tabs_module_constants;

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : execute_tab_id_(-1),
      all_frames_(false) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {
}

bool ExecuteCodeInTabFunction::RunImpl() {
  DictionaryValue* script_info;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &script_info));
  size_t number_of_value = script_info->size();
  if (number_of_value == 0) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  } else {
    bool has_code = script_info->HasKey(keys::kCodeKey);
    bool has_file = script_info->HasKey(keys::kFileKey);
    if (has_code && has_file) {
      error_ = keys::kMoreThanOneValuesError;
      return false;
    } else if (!has_code && !has_file) {
      error_ = keys::kNoCodeOrFileToExecuteError;
      return false;
    }
  }

  execute_tab_id_ = -1;
  Browser* browser = NULL;
  TabContentsWrapper* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  Value* tab_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &tab_value));
  if (tab_value->IsType(Value::TYPE_NULL)) {
    browser = GetCurrentBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }
    if (!ExtensionTabUtil::GetDefaultTab(browser, &contents, &execute_tab_id_))
      return false;
  } else {
    EXTENSION_FUNCTION_VALIDATE(tab_value->GetAsInteger(&execute_tab_id_));
    if (!ExtensionTabUtil::GetTabById(execute_tab_id_, profile(),
                                      include_incognito(),
                                      &browser, NULL, &contents, NULL)) {
      return false;
    }
  }

  DCHECK(browser);
  DCHECK(contents);

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  if (!GetExtension()->CanExecuteScriptOnPage(
          contents->tab_contents()->GetURL(), NULL, &error_)) {
    return false;
  }

  if (script_info->HasKey(keys::kAllFramesKey)) {
    if (!script_info->GetBoolean(keys::kAllFramesKey, &all_frames_))
      return false;
  }

  std::string code_string;
  if (script_info->HasKey(keys::kCodeKey)) {
    if (!script_info->GetString(keys::kCodeKey, &code_string))
      return false;
  }

  if (!code_string.empty()) {
    if (!Execute(code_string))
      return false;
    return true;
  }

  std::string relative_path;
  if (script_info->HasKey(keys::kFileKey)) {
    if (!script_info->GetString(keys::kFileKey, &relative_path))
      return false;
    resource_ = GetExtension()->GetResource(relative_path);
  }
  if (resource_.extension_root().empty() || resource_.relative_path().empty()) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  }

  scoped_refptr<FileReader> file_reader(new FileReader(
      resource_, NewCallback(this, &ExecuteCodeInTabFunction::DidLoadFile)));
  file_reader->Start();
  AddRef();  // Keep us alive until DidLoadFile is called.

  return true;
}

void ExecuteCodeInTabFunction::DidLoadFile(bool success,
                                           const std::string& data) {
  if (success) {
    Execute(data);
  } else {
#if defined(OS_POSIX)
    // TODO(viettrungluu): bug: there's no particular reason the path should be
    // UTF-8, in which case this may fail.
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kLoadFileError,
        resource_.relative_path().value());
#elif defined(OS_WIN)
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kLoadFileError,
        WideToUTF8(resource_.relative_path().value()));
#endif  // OS_WIN
    SendResponse(false);
  }
  Release();  // Balance the AddRef taken in RunImpl
}

bool ExecuteCodeInTabFunction::Execute(const std::string& code_string) {
  TabContentsWrapper* contents = NULL;
  Browser* browser = NULL;

  bool success = ExtensionTabUtil::GetTabById(
      execute_tab_id_, profile(), include_incognito(), &browser, NULL,
      &contents, NULL) && contents && browser;

  if (!success) {
    SendResponse(false);
    return false;
  }

  const Extension* extension = GetExtension();
  if (!extension) {
    SendResponse(false);
    return false;
  }

  bool is_js_code = true;
  std::string function_name = name();
  if (function_name == TabsInsertCSSFunction::function_name()) {
    is_js_code = false;
  } else if (function_name != TabsExecuteScriptFunction::function_name()) {
    DCHECK(false);
  }
  if (!contents->tab_contents()->ExecuteCode(request_id(), extension->id(),
      is_js_code, code_string, all_frames_)) {
    SendResponse(false);
    return false;
  }
  registrar_.Add(this, NotificationType::TAB_CODE_EXECUTED,
                 NotificationService::AllSources());
  AddRef();  // balanced in Observe()
  return true;
}

void ExecuteCodeInTabFunction::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  std::pair<int, bool>* result_details =
      Details<std::pair<int, bool> >(details).ptr();
  if (result_details->first == request_id()) {
    SendResponse(result_details->second);
    Release();  // balanced in Execute()
  }
}
