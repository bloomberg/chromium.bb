// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/browser_thread.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

// Shows a dialog box for selecting a file or a folder.
class FileManagerDialog
    : public SelectFileDialog,
      public HtmlDialogUIDelegate {

 public:
  explicit FileManagerDialog(Listener* listener);

  // Helper to convert numeric dialog type to a string.
  static std::string GetDialogTypeAsString(Type dialog_type);

  // Help to convert potential dialog arguments into json.
  static std::string GetArgumentsJson(
      Type type,
      const string16& title,
      const FilePath& default_path,
      const FileTypeInfo* file_types,
      int file_type_index,
      const FilePath::StringType& default_extension);

  // Thread callback.
  void CallbackOpen() {
    browser::ShowHtmlDialog(owner_window_,
                            ProfileManager::GetDefaultProfile(),
                            this);
  }

  // BaseShellDialog implementation.

  virtual bool IsRunning(gfx::NativeWindow owner_window) const {
    return owner_window_ == owner_window;
  }

  virtual void ListenerDestroyed() {
  }

  // SelectFileDialog implementation.

  virtual void SelectFile(Type type,
                          const string16& title,
                          const FilePath& default_path,
                          const FileTypeInfo* file_types,
                          int file_type_index,
                          const FilePath::StringType& default_extension,
                          gfx::NativeWindow owning_window,
                          void* params);

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

  // A callback to notify the delegate that the contents have gone
  // away. Only relevant if your dialog hosts code that calls
  // windows.close() and you've allowed that.  If the output parameter
  // is set to true, then the dialog is closed.  The default is false.
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
  }

  // A callback to allow the delegate to dictate that the window should not
  // have a title bar.  This is useful when presenting branded interfaces.
  virtual bool ShouldShowDialogTitle() const {
    return true;
  }

  // A callback to allow the delegate to inhibit context menu or show
  // customized menu.
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return true;
  }

 private:
  virtual ~FileManagerDialog() {}

  Listener* listener_;

  // True when opening in browser, otherwise in OOBE/login mode.
  bool browser_mode_;

  gfx::NativeWindow owner_window_;

  std::wstring title_;

  // Base url plus query string.
  GURL dialog_url_;

  // The base url for the file manager extension.
  static std::string s_extension_base_url_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerDialog);
};

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return new FileManagerDialog(listener);
}

// This is the "well known" url for the file manager extension from
// browser/resources/file_manager.  In the future we may provide a way to swap
// out this file manager for an aftermarket part, but not yet.
std::string FileManagerDialog::s_extension_base_url_ =
    "chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/main.html";

FileManagerDialog::FileManagerDialog(Listener* listener)
    : browser_mode_(true),
      owner_window_(0) {
  listener_ = listener;
}

void FileManagerDialog::SelectFile(
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

  dialog_url_ = GURL(s_extension_base_url_ + "?" + UrlEncodeString(
      GetArgumentsJson(type, title, default_path, file_types, file_type_index,
                       default_extension)));

  if (browser_mode_) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      browser->BrowserShowHtmlDialog(this, owner_window);
      return;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this, &FileManagerDialog::CallbackOpen));
}

// static
std::string FileManagerDialog::GetDialogTypeAsString(Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case SelectFileDialog::SELECT_NONE:
      type_str = "none";
      break;

    case SELECT_FOLDER:
      type_str = "folder";
      break;

    case SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;

    default:
      NOTREACHED();
  }

  return type_str;
}

// static
std::string FileManagerDialog::GetArgumentsJson(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension) {
  DictionaryValue arg_value;
  arg_value.SetString("type", GetDialogTypeAsString(type));
  arg_value.SetString("title", title);
  arg_value.SetString("defaultExtension", default_extension);

  ListValue* types_list = new ListValue();

  if (file_types) {
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      ListValue* extensions_list = new ListValue();
      for (size_t j = 0; j < file_types->extensions[i].size(); ++j) {
        extensions_list->Set(
            i, Value::CreateStringValue(file_types->extensions[i][j]));
      }

      DictionaryValue* dict = new DictionaryValue();
      dict->Set("extensions", extensions_list);

      if (i < file_types->extension_description_overrides.size()) {
        string16 desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == i));

      types_list->Set(i, dict);
    }
  }

  std::string rv;
  base::JSONWriter::Write(&arg_value, false, &rv);

  return rv;
}
