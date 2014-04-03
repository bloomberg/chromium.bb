// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_error_handler.h"

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_error_ui_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

ExtensionErrorHandler::ExtensionErrorHandler(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
}

ExtensionErrorHandler::~ExtensionErrorHandler() {
}

void ExtensionErrorHandler::GetLocalizedValues(
    content::WebUIDataSource* source) {
  source->AddString(
      "extensionErrorsShowMore",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_SHOW_MORE));
  source->AddString(
      "extensionErrorsShowFewer",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_SHOW_FEWER));
  source->AddString(
      "extensionErrorViewDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_VIEW_DETAILS));
  source->AddString(
      "extensionErrorViewManifest",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_VIEW_MANIFEST));
  source->AddString("extensionErrorOverlayDone",
                    l10n_util::GetStringUTF16(IDS_DONE));
  source->AddString(
      "extensionErrorOverlayContextUrl",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_CONTEXT));
  source->AddString(
      "extensionErrorOverlayStackTrace",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_STACK_TRACE));
  source->AddString(
      "extensionErrorOverlayAnonymousFunction",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_ANONYMOUS_FUNCTION));
  source->AddString(
      "extensionErrorOverlayLaunchDevtools",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_LAUNCH_DEVTOOLS));
  source->AddString(
      "extensionErrorOverlayContextUnknown",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_CONTEXT_UNKNOWN));
  source->AddString(
      "extensionErrorOverlayNoCodeToDisplay",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_NO_CODE_TO_DISPLAY));
}

void ExtensionErrorHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "extensionErrorRequestFileSource",
      base::Bind(&ExtensionErrorHandler::HandleRequestFileSource,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "extensionErrorOpenDevTools",
      base::Bind(&ExtensionErrorHandler::HandleOpenDevTools,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionErrorHandler::HandleRequestFileSource(
    const base::ListValue* args) {
  // There should only be one argument, a dictionary. Use this instead of a list
  // because it's more descriptive, harder to accidentally break with minor
  // modifications, and supports optional arguments more easily.
  CHECK_EQ(1u, args->GetSize());

  const base::DictionaryValue* dict = NULL;

  // Three required arguments: extension_id, path_suffix, and error_message.
  std::string extension_id;
  base::FilePath::StringType path_suffix_string;
  base::string16 error_message;

  if (!args->GetDictionary(0, &dict)) {
    NOTREACHED();
    return;
  }

  error_ui_util::HandleRequestFileSource(
      dict,
      Profile::FromWebUI(web_ui()),
      base::Bind(&ExtensionErrorHandler::OnFileSourceHandled,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionErrorHandler::OnFileSourceHandled(
    const base::DictionaryValue& source) {
  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionErrorOverlay.requestFileSourceResponse", source);
}

void ExtensionErrorHandler::HandleOpenDevTools(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::DictionaryValue* dict = NULL;

  if (!args->GetDictionary(0, &dict)) {
    NOTREACHED();
    return;
  }

  error_ui_util::HandleOpenDevTools(dict);
}

}  // namespace extensions
