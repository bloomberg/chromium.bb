// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_error_handler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/manifest_highlighter.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Keys for objects passed to and from extension error UI.
const char kFileTypeKey[] = "fileType";
const char kManifestFileType[] = "manifest";
const char kPathSuffixKey[] = "pathSuffix";
const char kTitleKey[] = "title";

const char kBeforeHighlightKey[] = "beforeHighlight";
const char kHighlightKey[] = "highlight";
const char kAfterHighlightKey[] = "afterHighlight";

// Populate a DictionaryValue with the highlighted portions for the callback to
// ExtensionErrorOverlay, given the components.
void HighlightDictionary(base::DictionaryValue* dict,
                         const ManifestHighlighter& highlighter) {
  std::string before_feature = highlighter.GetBeforeFeature();
  if (!before_feature.empty())
    dict->SetString(kBeforeHighlightKey, base::UTF8ToUTF16(before_feature));

  std::string feature = highlighter.GetFeature();
  if (!feature.empty())
    dict->SetString(kHighlightKey, base::UTF8ToUTF16(feature));

  std::string after_feature = highlighter.GetAfterFeature();
  if (!after_feature.empty())
    dict->SetString(kAfterHighlightKey, base::UTF8ToUTF16(after_feature));
}

}  // namespace

ExtensionErrorHandler::ExtensionErrorHandler(Profile* profile)
    : profile_(profile) {
}

ExtensionErrorHandler::~ExtensionErrorHandler() {
}

void ExtensionErrorHandler::GetLocalizedValues(
    content::WebUIDataSource* source) {
  source->AddString(
      "extensionErrorsManifestErrors",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_MANIFEST_ERRORS));
  source->AddString(
      "extensionErrorsShowMore",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_SHOW_MORE));
  source->AddString(
      "extensionErrorsShowFewer",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_SHOW_FEWER));
  source->AddString(
      "extensionErrorViewSource",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_VIEW_SOURCE));
}

void ExtensionErrorHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "extensionErrorRequestFileSource",
      base::Bind(&ExtensionErrorHandler::HandleRequestFileSource,
                 base::Unretained(this)));
}

void ExtensionErrorHandler::HandleRequestFileSource(
    const base::ListValue* args) {
  // There should only be one argument, a dictionary. Use this instead of a list
  // because it's more descriptive, harder to accidentally break with minor
  // modifications, and supports optional arguments more easily.
  CHECK_EQ(1u, args->GetSize());

  const base::DictionaryValue* dict = NULL;

  // Four required arguments: extension_id, path_suffix, error_message, and
  // file_type.
  std::string extension_id;
  base::FilePath::StringType path_suffix;
  base::string16 error_message;
  std::string file_type;

  if (!args->GetDictionary(0, &dict) ||
      !dict->GetString(kFileTypeKey, &file_type) ||
      !dict->GetString(kPathSuffixKey, &path_suffix) ||
      !dict->GetString(ExtensionError::kExtensionIdKey, &extension_id) ||
      !dict->GetString(ExtensionError::kMessageKey, &error_message)) {
    NOTREACHED();
    return;
  }

  const Extension* extension =
      ExtensionSystem::Get(Profile::FromWebUI(web_ui()))->
          extension_service()->GetExtensionById(extension_id,
                                                true /* include disabled */ );
  base::FilePath path = extension->path().Append(path_suffix);

  // Setting the title and the error message is the same for all file types.
  scoped_ptr<base::DictionaryValue> results(new base::DictionaryValue);
  results->SetString(kTitleKey,
                     base::UTF8ToUTF16(extension->name()) +
                         base::ASCIIToUTF16(": ") +
                         path.BaseName().LossyDisplayName());
  results->SetString(ExtensionError::kMessageKey, error_message);

  base::Closure closure;
  std::string* contents = NULL;

  if (file_type == kManifestFileType) {
    std::string manifest_key;
    if (!dict->GetString(ManifestError::kManifestKeyKey, &manifest_key)) {
      NOTREACHED();
      return;
    }

    // A "specific" location is optional.
    std::string specific;
    dict->GetString(ManifestError::kManifestSpecificKey, &specific);

    contents = new std::string;  // Owned by GetManifestFileCallback(    )
    closure = base::Bind(&ExtensionErrorHandler::GetManifestFileCallback,
                         base::Unretained(this),
                         base::Owned(results.release()),
                         manifest_key,
                         specific,
                         base::Owned(contents));
  } else {
    // currently, only manifest file types supported.
    NOTREACHED();
    return;
  }

  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::ReadFileToString),
                 path,
                 contents),
      closure);
}

void ExtensionErrorHandler::GetManifestFileCallback(
    base::DictionaryValue* results,
    const std::string& key,
    const std::string& specific,
    std::string* contents) {
  ManifestHighlighter highlighter(*contents, key, specific);
  HighlightDictionary(results, highlighter);
  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionErrorOverlay.requestFileSourceResponse", *results);
}

}  // namespace extensions
