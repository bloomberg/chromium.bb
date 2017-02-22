// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_loader_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/extensions/path_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Read a file to a string and return.
std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string for the manifest, and ignore it.
  base::ReadFileToString(path, &data);
  return data;
}

}  // namespace

ExtensionLoaderHandler::ExtensionLoaderHandler(Profile* profile)
    : profile_(profile),
      extension_error_reporter_observer_(this),
      ui_ready_(false),
      weak_ptr_factory_(this) {
  DCHECK(profile_);
  extension_error_reporter_observer_.Add(ExtensionErrorReporter::GetInstance());
}

ExtensionLoaderHandler::~ExtensionLoaderHandler() {
}

void ExtensionLoaderHandler::GetLocalizedValues(
    content::WebUIDataSource* source) {
  source->AddString(
      "extensionLoadErrorHeading",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_HEADING));
  source->AddString(
      "extensionLoadErrorMessage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE));
  source->AddString(
      "extensionLoadErrorRetry",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_RETRY));
  source->AddString(
      "extensionLoadErrorGiveUp",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_GIVE_UP));
  source->AddString(
      "extensionLoadCouldNotLoadManifest",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_COULD_NOT_LOAD_MANIFEST));
  source->AddString(
      "extensionLoadAdditionalFailures",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ADDITIONAL_FAILURES));
}

void ExtensionLoaderHandler::RegisterMessages() {
  // We observe WebContents in order to detect page refreshes, since notifying
  // the frontend of load failures must be delayed until the page finishes
  // loading. We never call Observe(NULL) because this object is constructed
  // on page load and persists between refreshes.
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());

  web_ui()->RegisterMessageCallback(
      "extensionLoaderRetry",
      base::Bind(&ExtensionLoaderHandler::HandleRetry,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "extensionLoaderIgnoreFailure",
      base::Bind(&ExtensionLoaderHandler::HandleIgnoreFailure,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "extensionLoaderDisplayFailures",
      base::Bind(&ExtensionLoaderHandler::HandleDisplayFailures,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionLoaderHandler::HandleRetry(const base::ListValue* args) {
  DCHECK(args->empty());
  const base::FilePath file_path = failed_paths_.back();
  failed_paths_.pop_back();
  LoadUnpackedExtension(file_path);
}

void ExtensionLoaderHandler::HandleIgnoreFailure(const base::ListValue* args) {
  DCHECK(args->empty());
  failed_paths_.pop_back();
}

void ExtensionLoaderHandler::HandleDisplayFailures(
    const base::ListValue* args) {
  DCHECK(args->empty());
  ui_ready_ = true;

  // Notify the frontend of any load failures that were triggered while the
  // chrome://extensions page was loading.
  if (!failures_.empty())
    NotifyFrontendOfFailure();
}

void ExtensionLoaderHandler::LoadUnpackedExtension(
      const base::FilePath& file_path) {
  scoped_refptr<UnpackedInstaller> installer = UnpackedInstaller::Create(
      ExtensionSystem::Get(profile_)->extension_service());

  // We do our own error handling, so we don't want a load failure to trigger
  // a dialog.
  installer->set_be_noisy_on_failure(false);

  installer->Load(file_path);
}

void ExtensionLoaderHandler::OnLoadFailure(
    content::BrowserContext* browser_context,
    const base::FilePath& file_path,
    const std::string& error) {
  // Only show errors from our browser context.
  if (web_ui()->GetWebContents()->GetBrowserContext() != browser_context)
    return;

  size_t line = 0u;
  size_t column = 0u;
  std::string regex =
      base::StringPrintf("%s  Line: (\\d+), column: (\\d+), .*",
                         manifest_errors::kManifestParseError);
  // If this was a JSON parse error, we can highlight the exact line with the
  // error. Otherwise, we should still display the manifest (for consistency,
  // reference, and so that if we ever make this really fancy and add an editor,
  // it's ready).
  //
  // This regex call can fail, but if it does, we just don't highlight anything.
  re2::RE2::FullMatch(error, regex, &line, &column);

  // This will read the manifest and call AddFailure with the read manifest
  // contents.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits().MayBlock().WithPriority(
          base::TaskPriority::USER_BLOCKING),
      base::Bind(&ReadFileToString, file_path.Append(kManifestFilename)),
      base::Bind(&ExtensionLoaderHandler::AddFailure,
                 weak_ptr_factory_.GetWeakPtr(), file_path, error, line));
}

void ExtensionLoaderHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  // In the event of a page reload, we ensure that the frontend is not notified
  // until the UI finishes loading, so we set |ui_ready_| to false. This is
  // balanced in HandleDisplayFailures, which is called when the frontend is
  // ready to receive failure notifications.
  if (navigation_handle->GetReloadType() != content::ReloadType::NONE)
    ui_ready_ = false;
}

void ExtensionLoaderHandler::AddFailure(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  failed_paths_.push_back(file_path);
  base::FilePath prettified_path = path_util::PrettifyPath(file_path);

  std::unique_ptr<base::DictionaryValue> manifest_value(
      new base::DictionaryValue());
  SourceHighlighter highlighter(manifest, line_number);
  // If the line number is 0, this highlights no regions, but still adds the
  // full manifest.
  highlighter.SetHighlightedRegions(manifest_value.get());

  std::unique_ptr<base::DictionaryValue> failure(new base::DictionaryValue());
  failure->Set("path",
               new base::StringValue(prettified_path.LossyDisplayName()));
  failure->Set("error", new base::StringValue(base::UTF8ToUTF16(error)));
  failure->Set("manifest", manifest_value.release());
  failures_.Append(std::move(failure));

  // Only notify the frontend if the frontend UI is ready.
  if (ui_ready_)
    NotifyFrontendOfFailure();
}

void ExtensionLoaderHandler::NotifyFrontendOfFailure() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "extensions.ExtensionLoader.notifyLoadFailed", failures_);
  failures_.Clear();
}

}  // namespace extensions
