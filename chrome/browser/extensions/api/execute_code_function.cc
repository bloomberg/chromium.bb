// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/execute_code_function.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/message_bundle.h"
#include "net/base/filename_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace keys = tabs_constants;
using api::tabs::InjectDetails;

ExecuteCodeFunction::ExecuteCodeFunction() {
}

ExecuteCodeFunction::~ExecuteCodeFunction() {
}

void ExecuteCodeFunction::DidLoadFile(bool success,
                                      const std::string& data) {

  if (!success || !details_->file) {
    DidLoadAndLocalizeFile(success, data);
    return;
  }

  ScriptExecutor::ScriptType script_type =
      ShouldInsertCSS() ? ScriptExecutor::CSS : ScriptExecutor::JAVASCRIPT;

  std::string extension_id;
  base::FilePath extension_path;
  std::string extension_default_locale;
  const Extension* extension = GetExtension();
  if (extension) {
    extension_id = extension->id();
    extension_path = extension->path();
    extension_default_locale = LocaleInfo::GetDefaultLocale(extension);
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&ExecuteCodeFunction::GetFileURLAndLocalizeCSS, this,
                  script_type,
                  data,
                  extension_id,
                  extension_path,
                  extension_default_locale));
}

void ExecuteCodeFunction::GetFileURLAndLocalizeCSS(
    ScriptExecutor::ScriptType script_type,
    const std::string& data,
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale) {

  std::string localized_data = data;
  // Check if the file is CSS and needs localization.
  if ((script_type == ScriptExecutor::CSS) &&
      !extension_id.empty() &&
      (data.find(MessageBundle::kMessageBegin) != std::string::npos)) {
    scoped_ptr<SubstitutionMap> localization_messages(
        file_util::LoadMessageBundleSubstitutionMap(
            extension_path, extension_id, extension_default_locale));

    // We need to do message replacement on the data, so it has to be mutable.
    std::string error;
    MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                         &localized_data,
                                                         &error);
  }

  file_url_ = net::FilePathToFileURL(resource_.GetFilePath());

  // Call back DidLoadAndLocalizeFile on the UI thread. The success parameter
  // is always true, because if loading had failed, we wouldn't have had
  // anything to localize.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                 true, localized_data));
}

void ExecuteCodeFunction::DidLoadAndLocalizeFile(bool success,
                                                 const std::string& data) {
  if (success) {
    if (!Execute(data))
      SendResponse(false);
  } else {
    // TODO(viettrungluu): bug: there's no particular reason the path should be
    // UTF-8, in which case this may fail.
    error_ = ErrorUtils::FormatErrorMessage(keys::kLoadFileError,
        resource_.relative_path().AsUTF8Unsafe());
    SendResponse(false);
  }
}

bool ExecuteCodeFunction::Execute(const std::string& code_string) {
  ScriptExecutor* executor = GetScriptExecutor();
  if (!executor)
    return false;

  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  ScriptExecutor::ScriptType script_type = ScriptExecutor::JAVASCRIPT;
  if (ShouldInsertCSS())
    script_type = ScriptExecutor::CSS;

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.get() && *details_->all_frames ?
          ScriptExecutor::ALL_FRAMES :
          ScriptExecutor::TOP_FRAME;

  ScriptExecutor::MatchAboutBlank match_about_blank =
      details_->match_about_blank.get() && *details_->match_about_blank ?
          ScriptExecutor::MATCH_ABOUT_BLANK :
          ScriptExecutor::DONT_MATCH_ABOUT_BLANK;

  UserScript::RunLocation run_at =
      UserScript::UNDEFINED;
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

  executor->ExecuteScript(
      extension->id(),
      script_type,
      code_string,
      frame_scope,
      match_about_blank,
      run_at,
      ScriptExecutor::ISOLATED_WORLD,
      IsWebView() ? ScriptExecutor::WEB_VIEW_PROCESS
                  : ScriptExecutor::DEFAULT_PROCESS,
      GetWebViewSrc(),
      file_url_,
      user_gesture_,
      has_callback() ? ScriptExecutor::JSON_SERIALIZED_RESULT
                     : ScriptExecutor::NO_RESULT,
      base::Bind(&ExecuteCodeFunction::OnExecuteCodeFinished, this));
  return true;
}

bool ExecuteCodeFunction::HasPermission() {
  return true;
}

bool ExecuteCodeFunction::RunAsync() {
  EXTENSION_FUNCTION_VALIDATE(Init());

  if (!details_->code.get() && !details_->file.get()) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  }
  if (details_->code.get() && details_->file.get()) {
    error_ = keys::kMoreThanOneValuesError;
    return false;
  }

  if (!CanExecuteScriptOnPage())
    return false;

  if (details_->code.get())
    return Execute(*details_->code);

  if (!details_->file.get())
    return false;
  resource_ = GetExtension()->GetResource(*details_->file);

  if (resource_.extension_root().empty() || resource_.relative_path().empty()) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  }

  int resource_id;
  if (ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager()->
      IsComponentExtensionResource(
          resource_.extension_root(), resource_.relative_path(),
          &resource_id)) {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    DidLoadFile(true, rb.GetRawDataResource(resource_id).as_string());
  } else {
    scoped_refptr<FileReader> file_reader(new FileReader(
        resource_, base::Bind(&ExecuteCodeFunction::DidLoadFile, this)));
    file_reader->Start();
  }

  return true;
}

void ExecuteCodeFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
  if (!error.empty())
    SetError(error);

  SendResponse(error.empty());
}

}  // namespace extensions
