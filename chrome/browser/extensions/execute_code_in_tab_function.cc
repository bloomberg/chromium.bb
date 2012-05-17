// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/execute_code_in_tab_function.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/file_reader.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using extensions::ScriptExecutor;

namespace keys = extension_tabs_module_constants;

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : execute_tab_id_(-1),
      all_frames_(false),
      run_at_(UserScript::DOCUMENT_IDLE) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

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

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  CHECK(browser);
  CHECK(contents);
  if (!GetExtension()->CanExecuteScriptOnPage(
          contents->web_contents()->GetURL(), NULL, &error_)) {
    return false;
  }

  if (script_info->HasKey(keys::kAllFramesKey)) {
    if (!script_info->GetBoolean(keys::kAllFramesKey, &all_frames_))
      return false;
  }

  if (script_info->HasKey(keys::kRunAtKey)) {
    std::string run_string;
    EXTENSION_FUNCTION_VALIDATE(script_info->GetString(
          keys::kRunAtKey, &run_string));

    if (run_string == extension_manifest_values::kRunAtDocumentStart)
      run_at_ = UserScript::DOCUMENT_START;
    else if (run_string == extension_manifest_values::kRunAtDocumentEnd)
      run_at_ = UserScript::DOCUMENT_END;
    else if (run_string == extension_manifest_values::kRunAtDocumentIdle)
      run_at_ = UserScript::DOCUMENT_IDLE;
    else
      EXTENSION_FUNCTION_VALIDATE(false);
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
      resource_, base::Bind(&ExecuteCodeInTabFunction::DidLoadFile, this)));
  file_reader->Start();

  return true;
}

void ExecuteCodeInTabFunction::OnExecuteCodeFinished(bool success,
                                                     const std::string& error) {
  if (!error.empty()) {
    CHECK(!success);
    error_ = error;
  }

  SendResponse(success);
}

void ExecuteCodeInTabFunction::DidLoadFile(bool success,
                                           const std::string& data) {
  std::string function_name = name();
  const Extension* extension = GetExtension();

  // Check if the file is CSS and needs localization.
  if (success &&
      function_name == TabsInsertCSSFunction::function_name() &&
      extension != NULL &&
      data.find(ExtensionMessageBundle::kMessageBegin) != std::string::npos) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ExecuteCodeInTabFunction::LocalizeCSS, this,
                   data,
                   extension->id(),
                   extension->path(),
                   extension->default_locale()));
  } else {
    DidLoadAndLocalizeFile(success, data);
  }
}

void ExecuteCodeInTabFunction::LocalizeCSS(
    const std::string& data,
    const std::string& extension_id,
    const FilePath& extension_path,
    const std::string& extension_default_locale) {
  scoped_ptr<SubstitutionMap> localization_messages(
      extension_file_util::LoadExtensionMessageBundleSubstitutionMap(
          extension_path, extension_id, extension_default_locale));

  // We need to do message replacement on the data, so it has to be mutable.
  std::string css_data = data;
  std::string error;
  ExtensionMessageBundle::ReplaceMessagesWithExternalDictionary(
      *localization_messages, &css_data, &error);

  // Call back DidLoadAndLocalizeFile on the UI thread. The success parameter
  // is always true, because if loading had failed, we wouldn't have had
  // anything to localize.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExecuteCodeInTabFunction::DidLoadAndLocalizeFile, this,
                 true, css_data));
}

void ExecuteCodeInTabFunction::DidLoadAndLocalizeFile(bool success,
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

  ScriptExecutor::ScriptType script_type = ScriptExecutor::JAVASCRIPT;
  std::string function_name = name();
  if (function_name == TabsInsertCSSFunction::function_name()) {
    script_type = ScriptExecutor::CSS;
  } else if (function_name != TabsExecuteScriptFunction::function_name()) {
    NOTREACHED();
  }

  contents->extension_tab_helper()->script_executor()->ExecuteScript(
      extension->id(),
      script_type,
      code_string,
      all_frames_ ? ScriptExecutor::ALL_FRAMES : ScriptExecutor::TOP_FRAME,
      run_at_,
      ScriptExecutor::ISOLATED_WORLD,
      base::Bind(&ExecuteCodeInTabFunction::OnExecuteCodeFinished, this));
  return true;
}
