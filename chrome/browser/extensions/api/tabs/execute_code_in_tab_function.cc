// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/execute_code_in_tab_function.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/file_reader.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/message_bundle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using extensions::api::tabs::InjectDetails;
using extensions::ScriptExecutor;
using extensions::UserScript;

namespace keys = extensions::tabs_constants;

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : execute_tab_id_(-1) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

bool ExecuteCodeInTabFunction::HasPermission() {
  if (Init() &&
      extension_->HasAPIPermissionForTab(execute_tab_id_,
                                         extensions::APIPermission::kTab)) {
    return true;
  }
  return ExtensionFunction::HasPermission();
}

bool ExecuteCodeInTabFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(Init());

  if (!details_->code.get() && !details_->file.get()) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  }
  if (details_->code.get() && details_->file.get()) {
    error_ = keys::kMoreThanOneValuesError;
    return false;
  }

  TabContents* contents = NULL;

  // If |tab_id| is specified, look for the tab. Otherwise default to selected
  // tab in the current window.
  CHECK_GE(execute_tab_id_, 0);
  if (!ExtensionTabUtil::GetTabById(execute_tab_id_, profile(),
                                    include_incognito(),
                                    NULL, NULL, &contents, NULL)) {
    return false;
  }

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  CHECK(contents);
  if (!GetExtension()->CanExecuteScriptOnPage(
          contents->web_contents()->GetURL(),
          contents->web_contents()->GetURL(),
          execute_tab_id_,
          NULL,
          &error_)) {
    return false;
  }

  if (details_->code.get())
    return Execute(*details_->code);

  CHECK(details_->file.get());
  resource_ = GetExtension()->GetResource(*details_->file);

  if (resource_.extension_root().empty() || resource_.relative_path().empty()) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  }

  scoped_refptr<FileReader> file_reader(new FileReader(
      resource_, base::Bind(&ExecuteCodeInTabFunction::DidLoadFile, this)));
  file_reader->Start();

  return true;
}

void ExecuteCodeInTabFunction::OnExecuteCodeFinished(const std::string& error,
                                                     int32 on_page_id,
                                                     const GURL& on_url,
                                                     const ListValue& result) {
  if (!error.empty())
    SetError(error);

  SendResponse(error.empty());
}

void TabsExecuteScriptFunction::OnExecuteCodeFinished(const std::string& error,
                                                      int32 on_page_id,
                                                      const GURL& on_url,
                                                      const ListValue& result) {
  if (error.empty())
    SetResult(result.DeepCopy());
  ExecuteCodeInTabFunction::OnExecuteCodeFinished(error, on_page_id, on_url,
                                                  result);
}

bool ExecuteCodeInTabFunction::Init() {
  if (details_.get())
    return true;

  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  args_->GetInteger(0, &tab_id);

  // |details| are not optional.
  DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return false;
  scoped_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return false;

  // If the tab ID is -1 then it needs to be converted to the currently active
  // tab's ID.
  if (tab_id == -1) {
    Browser* browser = GetCurrentBrowser();
    if (!browser)
      return false;
    TabContents* tab_contents = NULL;
    if (!ExtensionTabUtil::GetDefaultTab(browser, &tab_contents, &tab_id))
      return false;
  }

  execute_tab_id_ = tab_id;
  details_ = details.Pass();
  return true;
}

void ExecuteCodeInTabFunction::DidLoadFile(bool success,
                                           const std::string& data) {
  std::string function_name = name();
  const extensions::Extension* extension = GetExtension();

  // Check if the file is CSS and needs localization.
  if (success &&
      function_name == TabsInsertCSSFunction::function_name() &&
      extension != NULL &&
      data.find(
          extensions::MessageBundle::kMessageBegin) != std::string::npos) {
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
      extension_file_util::LoadMessageBundleSubstitutionMap(
          extension_path, extension_id, extension_default_locale));

  // We need to do message replacement on the data, so it has to be mutable.
  std::string css_data = data;
  std::string error;
  extensions::MessageBundle::ReplaceMessagesWithExternalDictionary(
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
    if (!Execute(data))
      SendResponse(false);
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
  TabContents* contents = NULL;
  Browser* browser = NULL;

  bool success = ExtensionTabUtil::GetTabById(
      execute_tab_id_, profile(), include_incognito(), &browser, NULL,
      &contents, NULL) && contents && browser;

  if (!success)
    return false;

  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return false;

  ScriptExecutor::ScriptType script_type = ScriptExecutor::JAVASCRIPT;
  std::string function_name = name();
  if (function_name == TabsInsertCSSFunction::function_name()) {
    script_type = ScriptExecutor::CSS;
  } else if (function_name != TabsExecuteScriptFunction::function_name()) {
    NOTREACHED();
  }

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.get() && *details_->all_frames ?
          ScriptExecutor::ALL_FRAMES :
          ScriptExecutor::TOP_FRAME;

  UserScript::RunLocation run_at = UserScript::UNDEFINED;
  switch (details_->run_at) {
    case InjectDetails::RUN_AT_NONE:
    case InjectDetails::RUN_AT_DOCUMENT_IDLE:
      run_at = UserScript::DOCUMENT_IDLE;
      break;
    case InjectDetails::RUN_AT_DOCUMENT_START:
      run_at = UserScript::DOCUMENT_START;
      break;
    case InjectDetails::RUN_AT_DOCUMENT_END:
      run_at = UserScript::DOCUMENT_END;
      break;
  }
  CHECK_NE(UserScript::UNDEFINED, run_at);

  extensions::TabHelper::FromWebContents(contents->web_contents())->
      script_executor()->ExecuteScript(
          extension->id(),
          script_type,
          code_string,
          frame_scope,
          run_at,
          ScriptExecutor::ISOLATED_WORLD,
          base::Bind(&ExecuteCodeInTabFunction::OnExecuteCodeFinished, this));
  return true;
}
