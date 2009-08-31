// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/win_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/shell_dialogs.h"
#include "grit/generated_resources.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace {

class PackDialogContent;

// Manages packing an extension on the file thread and reporting the result
// back to the UI.
class PackExtensionJob : public base::RefCounted<PackExtensionJob> {
 public:
  PackExtensionJob(PackDialogContent* dialog_content,
                   const FilePath& root_directory,
                   const FilePath& key_file);

  // PackDialogContent calls this when it is closing, so that PackExtensionJob
  // doesn't try to call back to the UI after it's destroyed.
  void OnDialogClosed();

 private:
  void RunOnFileThread();
  void ReportSuccessOnUIThread();
  void ReportFailureOnUIThread(const std::string& error);

  MessageLoop* ui_loop_;
  MessageLoop* file_loop_;
  PackDialogContent* dialog_content_;
  FilePath root_directory_;
  FilePath key_file_;
  FilePath crx_file_out_;
  FilePath key_file_out_;

  DISALLOW_COPY_AND_ASSIGN(PackExtensionJob);
};


// Puts up the the pack dialog, which has this basic layout:
//
// Select the extension to pack.
//
// Extension root:      [            ] [browse]
// Extension key file:  [            ] [browse]
//
//                                [ok] [cancel]
class PackDialogContent
  : public views::View,
    public SelectFileDialog::Listener,
    public views::ButtonListener,
    public views::DialogDelegate {
 public:
  PackDialogContent() {
    using views::GridLayout;

    // Setup the layout.
    views::GridLayout* layout = CreatePanelGridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0);
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    views::Label* heading = new views::Label(
        l10n_util::GetString(IDS_EXTENSION_PACK_DIALOG_HEADING));
    heading->SetMultiLine(true);
    heading->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(heading, 5, 1, GridLayout::FILL, GridLayout::LEADING);

    layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

    layout->StartRow(0, 0);
    layout->AddView(new views::Label(l10n_util::GetString(
        IDS_EXTENSION_PACK_DIALOG_ROOT_DIRECTORY_LABEL)));
    extension_root_textbox_ = new views::Textfield();
    extension_root_textbox_->set_default_width_in_chars(32);
    layout->AddView(extension_root_textbox_);
    extension_root_button_ = new views::NativeButton(this, l10n_util::GetString(
        IDS_EXTENSION_PACK_DIALOG_BROWSE));
    layout->AddView(extension_root_button_);

    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    layout->StartRow(0, 0);
    layout->AddView(new views::Label(l10n_util::GetString(
        IDS_EXTENSION_PACK_DIALOG_PRIVATE_KEY_LABEL)));
    private_key_textbox_ = new views::Textfield();
    private_key_textbox_->set_default_width_in_chars(32);
    layout->AddView(private_key_textbox_);
    private_key_button_ = new views::NativeButton(this, l10n_util::GetString(
      IDS_EXTENSION_PACK_DIALOG_BROWSE));
    layout->AddView(private_key_button_);
  }

  void OnPackSuccess(const FilePath& crx_file, const FilePath& pem_file) {
    std::wstring message;
    if (!pem_file.empty()) {
      message = l10n_util::GetStringF(
          IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_NEW,
          crx_file.ToWStringHack().c_str(),
          pem_file.ToWStringHack().c_str());
    } else {
      message = l10n_util::GetStringF(
          IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_UPDATE,
          crx_file.ToWStringHack().c_str());
    }
    win_util::MessageBox(GetWindow()->GetNativeWindow(), message,
        l10n_util::GetString(IDS_EXTENSION_PACK_DIALOG_SUCCESS_TITLE),
        MB_OK | MB_SETFOREGROUND);
    GetWindow()->Close();
}

  void OnPackFailure(const std::wstring& error) {
    win_util::MessageBox(GetWindow()->GetNativeWindow(), error,
        l10n_util::GetString(IDS_EXTENSION_PACK_DIALOG_FAILURE_TITLE),
        MB_OK | MB_SETFOREGROUND);
  }

 private:
  // DialogDelegate
  virtual bool Accept() {
    FilePath root_directory = FilePath::FromWStringHack(
        extension_root_textbox_->text());
    FilePath key_file = FilePath::FromWStringHack(private_key_textbox_->text());

    if (root_directory.empty()) {
      if (extension_root_textbox_->text().empty()) {
        OnPackFailure(l10n_util::GetString(
            IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED));
      } else {
        OnPackFailure(l10n_util::GetString(
            IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID));
      }

      return false;
    }

    if (!private_key_textbox_->text().empty() && key_file.empty()) {
      OnPackFailure(l10n_util::GetString(
          IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID));
      return false;
    }

    pack_job_ = new PackExtensionJob(this, root_directory, key_file);

    // Prevent the dialog from closing because PackExtensionJob is asynchronous.
    // We need to wait to find out if it succeeded before closing the window.
    return false;
  }

  virtual void OnClose() {
    if (pack_job_)
      pack_job_->OnDialogClosed();
  }

  // WindowDelegate
  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetString(IDS_EXTENSION_PACK_DIALOG_TITLE);
  }
  virtual views::View* GetContentsView() {
    return this;
  }

  // ButtonListener
  virtual void ButtonPressed(views::Button* sender) {
    file_dialog_ = SelectFileDialog::Create(this);

    if (sender == extension_root_button_) {
      file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
        l10n_util::GetString(IDS_EXTENSION_PACK_DIALOG_SELECT_ROOT),
        FilePath::FromWStringHack(extension_root_textbox_->text()),
        NULL, 0, FILE_PATH_LITERAL(""),
        GetWindow()->GetNativeWindow(),
        extension_root_textbox_);
    } else {
      static SelectFileDialog::FileTypeInfo info;
      info.extensions.push_back(std::vector<FilePath::StringType>());
      info.extensions.front().push_back(FILE_PATH_LITERAL("pem"));
      info.extension_description_overrides.push_back(l10n_util::GetString(
          IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
      info.include_all_files = true;

      file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
        l10n_util::GetString(IDS_EXTENSION_PACK_DIALOG_SELECT_KEY),
        FilePath::FromWStringHack(private_key_textbox_->text()),
        &info, 1, FILE_PATH_LITERAL(""),
        GetWindow()->GetNativeWindow(),
        private_key_textbox_);
    }
  }

  // SelectFileDialog::Listener
  virtual void MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {};
  virtual void FileSelectionCanceled(void* params) {};
  virtual void FileSelected(const FilePath& path, int index, void* params) {
    static_cast<views::Textfield*>(params)->SetText(path.ToWStringHack());
  }

  views::Textfield* extension_root_textbox_;
  views::Textfield* private_key_textbox_;
  views::NativeButton* extension_root_button_;
  views::NativeButton* private_key_button_;

  scoped_refptr<SelectFileDialog> file_dialog_;
  scoped_refptr<PackExtensionJob> pack_job_;

  DISALLOW_COPY_AND_ASSIGN(PackDialogContent);
};


PackExtensionJob::PackExtensionJob(PackDialogContent* dialog_content,
                                   const FilePath& root_directory,
                                   const FilePath& key_file)
    : ui_loop_(MessageLoop::current()), dialog_content_(dialog_content),
      root_directory_(root_directory), key_file_(key_file) {
  ChromeThread::GetMessageLoop(ChromeThread::FILE)->PostTask(FROM_HERE,
      NewRunnableMethod(this, &PackExtensionJob::RunOnFileThread));
}

void PackExtensionJob::OnDialogClosed() {
  dialog_content_ = NULL;
}

void PackExtensionJob::RunOnFileThread() {
  crx_file_out_ = root_directory_.ReplaceExtension(FILE_PATH_LITERAL("crx"));

  if (key_file_.empty())
    key_file_out_ = root_directory_.ReplaceExtension(FILE_PATH_LITERAL("pem"));

  // TODO(aa): Need to internationalize the errors that ExtensionCreator
  // returns.
  ExtensionCreator creator;
  if (creator.Run(root_directory_, crx_file_out_, key_file_, key_file_out_)) {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &PackExtensionJob::ReportSuccessOnUIThread));
  } else {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &PackExtensionJob::ReportFailureOnUIThread, creator.error_message()));
  }
}

void PackExtensionJob::ReportSuccessOnUIThread() {
  if (dialog_content_)
    dialog_content_->OnPackSuccess(crx_file_out_, key_file_out_);
}

void PackExtensionJob::ReportFailureOnUIThread(const std::string& error) {
  if (dialog_content_)
    dialog_content_->OnPackFailure(UTF8ToWide(error));
}

} // namespace

// static
void ExtensionsDOMHandler::ShowPackDialog() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  BrowserWindow* window = browser->window();
  if (!window)
    return;

  views::Window::CreateChromeWindow(window->GetNativeHandle(),
      gfx::Rect(400, 0), new PackDialogContent())->Show();
}
