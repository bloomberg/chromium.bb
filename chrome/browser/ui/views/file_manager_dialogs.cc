// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "views/window/window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

// Shows a dialog box for selecting a file or a folder.
class FileManagerDialog
    : public SelectFileDialog,
      public HtmlDialogUIDelegate {

 public:
  explicit FileManagerDialog(Listener* listener);

  void CreateHtmlDialogView(Profile* profile, void* params) {
    HtmlDialogView* html_view = new HtmlDialogView(profile, this);
    browser::CreateViewsWindow(owner_window_, gfx::Rect(), html_view);
    html_view->InitDialog();
    html_view->window()->Show();
    tab_id_ = html_view->tab_contents()->controller().session_id().id();

    // Register our callback and associate it with our tab.
    FileDialogFunction::Callback::Add(tab_id_, listener_, html_view, params);
  }

  // BaseShellDialog implementation.

  virtual bool IsRunning(gfx::NativeWindow owner_window) const {
    return owner_window_ == owner_window;
  }

  virtual void ListenerDestroyed() {
    listener_ = NULL;
    FileDialogFunction::Callback::Remove(tab_id_);
  }

  // SelectFileDialog implementation.
  virtual void set_browser_mode(bool value) {
    browser_mode_ = value;
  }

  // HtmlDialogUIDelegate implementation.

  virtual bool IsDialogModal() const {
    return true;
  }

  virtual std::wstring GetDialogTitle() const {
    return title_;
  }

  virtual GURL GetDialogContentURL() const {
    return dialog_url_;
  }

  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const {
  }

  // Get the size of the dialog.
  virtual void GetDialogSize(gfx::Size* size) const {
    size->set_width(720);
    size->set_height(580);
  }

  virtual std::string GetDialogArgs() const {
    return "";
  }

  // A callback to notify the delegate that the dialog closed.
  virtual void OnDialogClosed(const std::string& json_retval) {
    owner_window_ = NULL;
  }

  virtual void OnWindowClosed() {
    // Directly closing the window selects no files.
    const FileDialogFunction::Callback& callback =
        FileDialogFunction::Callback::Find(tab_id_);
    if (!callback.IsNull())
      callback.listener()->FileSelectionCanceled(callback.params());
  }

  // A callback to notify the delegate that the contents have gone
  // away. Only relevant if your dialog hosts code that calls
  // windows.close() and you've allowed that.  If the output parameter
  // is set to true, then the dialog is closed.  The default is false.
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
    *out_close_dialog = true;
  }

  // A callback to allow the delegate to dictate that the window should not
  // have a title bar.  This is useful when presenting branded interfaces.
  virtual bool ShouldShowDialogTitle() const {
    return false;
  }

  // A callback to allow the delegate to inhibit context menu or show
  // customized menu.
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return true;
  }

 protected:
  // SelectFileDialog implementation.
  virtual void SelectFileImpl(Type type,
                              const string16& title,
                              const FilePath& default_path,
                              const FileTypeInfo* file_types,
                              int file_type_index,
                              const FilePath::StringType& default_extension,
                              gfx::NativeWindow owning_window,
                              void* params);

 private:
  virtual ~FileManagerDialog() {}

  int32 tab_id_;

  // True when opening in browser, otherwise in OOBE/login mode.
  bool browser_mode_;

  gfx::NativeWindow owner_window_;

  std::wstring title_;

  // Base url plus query string.
  GURL dialog_url_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerDialog);
};

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new FileManagerDialog(listener);
}

FileManagerDialog::FileManagerDialog(Listener* listener)
    : SelectFileDialog(listener),
      tab_id_(0),
      browser_mode_(true),
      owner_window_(0) {
}

void FileManagerDialog::SelectFileImpl(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    void* params) {

  if (owner_window_) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }

  title_ = UTF16ToWide(title);
  owner_window_ = owner_window;

  dialog_url_ = FileManagerUtil::GetFileBrowserUrlWithParams(type, title,
      default_path, file_types, file_type_index, default_extension);

  if (browser_mode_) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      DCHECK_EQ(browser->type(), Browser::TYPE_TABBED);
      CreateHtmlDialogView(browser->profile(), params);
      return;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this,
                        &FileManagerDialog::CreateHtmlDialogView,
                        ProfileManager::GetDefaultProfile(), params));
}

