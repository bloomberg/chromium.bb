// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_loader_handler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/path_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/zipfile_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "third_party/re2/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

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

class ExtensionLoaderHandler::FileHelper
    : public ui::SelectFileDialog::Listener {
 public:
  explicit FileHelper(ExtensionLoaderHandler* loader_handler);
  virtual ~FileHelper();

  // Create a FileDialog for the user to select the unpacked extension
  // directory.
  void ChooseFile();

 private:
  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void MultiFilesSelected(
      const std::vector<base::FilePath>& files, void* params) OVERRIDE;

  // The associated ExtensionLoaderHandler. Weak, but guaranteed to be alive,
  // as it owns this object.
  ExtensionLoaderHandler* loader_handler_;

  // The dialog used to pick a directory when loading an unpacked extension.
  scoped_refptr<ui::SelectFileDialog> load_extension_dialog_;

  // The last selected directory, so we can start in the same spot.
  base::FilePath last_unpacked_directory_;

  // The title of the dialog.
  base::string16 title_;

  DISALLOW_COPY_AND_ASSIGN(FileHelper);
};

ExtensionLoaderHandler::FileHelper::FileHelper(
    ExtensionLoaderHandler* loader_handler)
    : loader_handler_(loader_handler),
      title_(l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY)) {
}

ExtensionLoaderHandler::FileHelper::~FileHelper() {
  // There may be a pending file dialog; inform it the listener is destroyed so
  // it doesn't try and call back.
  if (load_extension_dialog_.get())
    load_extension_dialog_->ListenerDestroyed();
}

void ExtensionLoaderHandler::FileHelper::ChooseFile() {
  static const int kFileTypeIndex = 0;  // No file type information to index.
  static const ui::SelectFileDialog::Type kSelectType =
      ui::SelectFileDialog::SELECT_FOLDER;

  if (!load_extension_dialog_.get()) {
    load_extension_dialog_ = ui::SelectFileDialog::Create(
        this,
        new ChromeSelectFilePolicy(
            loader_handler_->web_ui()->GetWebContents()));
  }

  load_extension_dialog_->SelectFile(
      kSelectType,
      title_,
      last_unpacked_directory_,
      NULL,
      kFileTypeIndex,
      base::FilePath::StringType(),
      loader_handler_->web_ui()->GetWebContents()->GetTopLevelNativeWindow(),
      NULL);

  content::RecordComputedAction("Options_LoadUnpackedExtension");
}

void ExtensionLoaderHandler::FileHelper::FileSelected(
    const base::FilePath& path, int index, void* params) {
  loader_handler_->LoadUnpackedExtensionImpl(path);
}

void ExtensionLoaderHandler::FileHelper::MultiFilesSelected(
      const std::vector<base::FilePath>& files, void* params) {
  NOTREACHED();
}

ExtensionLoaderHandler::ExtensionLoaderHandler(Profile* profile)
    : profile_(profile),
      file_helper_(new FileHelper(this)),
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
      "extensionLoaderLoadUnpacked",
      base::Bind(&ExtensionLoaderHandler::HandleLoadUnpacked,
                 weak_ptr_factory_.GetWeakPtr()));
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

void ExtensionLoaderHandler::HandleLoadUnpacked(const base::ListValue* args) {
  DCHECK(args->empty());
  file_helper_->ChooseFile();
}

void ExtensionLoaderHandler::HandleRetry(const base::ListValue* args) {
  DCHECK(args->empty());
  const base::FilePath file_path = failed_paths_.back();
  failed_paths_.pop_back();
  LoadUnpackedExtensionImpl(file_path);
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

void ExtensionLoaderHandler::LoadUnpackedExtensionImpl(
    const base::FilePath& file_path) {
  if (EndsWith(file_path.AsUTF16Unsafe(),
               base::ASCIIToUTF16(".zip"),
               false /* case insensitive */)) {
    scoped_refptr<ZipFileInstaller> installer = ZipFileInstaller::Create(
        ExtensionSystem::Get(profile_)->extension_service());

    // We do our own error handling, so we don't want a load failure to trigger
    // a dialog.
    installer->set_be_noisy_on_failure(false);

    installer->LoadFromZipFile(file_path);
  } else {
    scoped_refptr<UnpackedInstaller> installer = UnpackedInstaller::Create(
        ExtensionSystem::Get(profile_)->extension_service());

    // We do our own error handling, so we don't want a load failure to trigger
    // a dialog.
    installer->set_be_noisy_on_failure(false);

    installer->Load(file_path);
  }
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
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ReadFileToString, file_path.Append(kManifestFilename)),
      base::Bind(&ExtensionLoaderHandler::AddFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 error,
                 line));
}

void ExtensionLoaderHandler::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  // In the event of a page reload, we ensure that the frontend is not notified
  // until the UI finishes loading, so we set |ui_ready_| to false. This is
  // balanced in HandleDisplayFailures, which is called when the frontend is
  // ready to receive failure notifications.
  if (reload_type != content::NavigationController::NO_RELOAD)
    ui_ready_ = false;
}

void ExtensionLoaderHandler::AddFailure(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  failed_paths_.push_back(file_path);
  base::FilePath prettified_path = path_util::PrettifyPath(file_path);

  scoped_ptr<base::DictionaryValue> manifest_value(new base::DictionaryValue());
  SourceHighlighter highlighter(manifest, line_number);
  // If the line number is 0, this highlights no regions, but still adds the
  // full manifest.
  highlighter.SetHighlightedRegions(manifest_value.get());

  scoped_ptr<base::DictionaryValue> failure(new base::DictionaryValue());
  failure->Set("path",
               new base::StringValue(prettified_path.LossyDisplayName()));
  failure->Set("reason", new base::StringValue(base::UTF8ToUTF16(error)));
  failure->Set("manifest", manifest_value.release());
  failures_.Append(failure.release());

  // Only notify the frontend if the frontend UI is ready.
  if (ui_ready_)
    NotifyFrontendOfFailure();
}

void ExtensionLoaderHandler::NotifyFrontendOfFailure() {
  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionLoader.notifyLoadFailed",
      failures_);
  failures_.Clear();
}

}  // namespace extensions
