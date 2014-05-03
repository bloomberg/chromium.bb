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
#include "base/values.h"
#include "chrome/browser/extensions/unpacked_installer.h"
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
      weak_ptr_factory_(this) {
  DCHECK(profile_);
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
}

void ExtensionLoaderHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "extensionLoaderLoadUnpacked",
      base::Bind(&ExtensionLoaderHandler::HandleLoadUnpacked,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "extensionLoaderRetry",
      base::Bind(&ExtensionLoaderHandler::HandleRetry,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionLoaderHandler::HandleLoadUnpacked(const base::ListValue* args) {
  DCHECK(args->empty());
  file_helper_->ChooseFile();
}

void ExtensionLoaderHandler::HandleRetry(const base::ListValue* args) {
  DCHECK(args->empty());
  LoadUnpackedExtensionImpl(failed_path_);
}

void ExtensionLoaderHandler::LoadUnpackedExtensionImpl(
    const base::FilePath& file_path) {
  scoped_refptr<UnpackedInstaller> installer = UnpackedInstaller::Create(
      ExtensionSystem::Get(profile_)->extension_service());
  installer->set_on_failure_callback(
      base::Bind(&ExtensionLoaderHandler::OnLoadFailure,
                 weak_ptr_factory_.GetWeakPtr()));

  // We do our own error handling, so we don't want a load failure to trigger
  // a dialog.
  installer->set_be_noisy_on_failure(false);

  installer->Load(file_path);
}

void ExtensionLoaderHandler::OnLoadFailure(const base::FilePath& file_path,
                                           const std::string& error) {
  failed_path_ = file_path;
  size_t line = 0u;
  size_t column = 0u;
  std::string regex =
      base::StringPrintf("%s  Line: (\\d+), column: (\\d+), Syntax error.",
                         manifest_errors::kManifestParseError);
  // If this was a JSON parse error, we can highlight the exact line with the
  // error. Otherwise, we should still display the manifest (for consistency,
  // reference, and so that if we ever make this really fancy and add an editor,
  // it's ready).
  //
  // This regex call can fail, but if it does, we just don't highlight anything.
  re2::RE2::FullMatch(error, regex, &line, &column);

  // This will read the manifest and call NotifyFrontendOfFailure with the read
  // manifest contents.
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ReadFileToString, file_path.Append(kManifestFilename)),
      base::Bind(&ExtensionLoaderHandler::NotifyFrontendOfFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 error,
                 line));
}

void ExtensionLoaderHandler::NotifyFrontendOfFailure(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  base::StringValue file_value(file_path.LossyDisplayName());
  base::StringValue error_value(base::UTF8ToUTF16(error));

  base::DictionaryValue manifest_value;
  SourceHighlighter highlighter(manifest, line_number);
  // If the line number is 0, this highlights no regions, but still adds the
  // full manifest.
  highlighter.SetHighlightedRegions(&manifest_value);

  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionLoader.notifyLoadFailed",
      file_value,
      error_value,
      manifest_value);
}

}  // namespace extensions
