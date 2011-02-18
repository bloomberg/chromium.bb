// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shell_dialogs.h"

#include "base/callback.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

namespace {

const char kKeyNamePath[] = "path";
const int kSaveCompletePageIndex = 2;

}  // namespace

// Implementation of SelectFileDialog that shows an UI for choosing a file
// or folder using FileBrowseUI.
class SelectFileDialogImpl : public SelectFileDialog {
 public:
  explicit SelectFileDialogImpl(Listener* listener);

  // BaseShellDialog implementation.
  virtual bool IsRunning(gfx::NativeWindow parent_window) const;
  virtual void ListenerDestroyed();

  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
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

 private:
  virtual ~SelectFileDialogImpl();

  class FileBrowseDelegate : public HtmlDialogUIDelegate {
   public:
    FileBrowseDelegate(SelectFileDialogImpl* owner,
                       Type type,
                       const std::wstring& title,
                       const FilePath& default_path,
                       const FileTypeInfo* file_types,
                       int file_type_index,
                       const FilePath::StringType& default_extension,
                       gfx::NativeWindow parent,
                       void* params);

    // Owner of this FileBrowseDelegate.
    scoped_refptr<SelectFileDialogImpl> owner_;

    // Parent window.
    gfx::NativeWindow parent_;

    // The type of dialog we are showing the user.
    Type type_;

    // The dialog title.
    std::wstring title_;

    // Default path of the file dialog.
    FilePath default_path_;

    // The file filters.
    FileTypeInfo file_types_;

    // The index of the default selected file filter.
    // Note: This starts from 1, not 0.
    int file_type_index_;

    // Default extension to be added to file if user does not type one.
    FilePath::StringType default_extension_;

    // Associated user data.
    void* params_;

   private:
    ~FileBrowseDelegate();

    // Overridden from HtmlDialogUI::Delegate:
    virtual bool IsDialogModal() const;
    virtual std::wstring GetDialogTitle() const;
    virtual GURL GetDialogContentURL() const;
    virtual void GetWebUIMessageHandlers(
        std::vector<WebUIMessageHandler*>* handlers) const;
    virtual void GetDialogSize(gfx::Size* size) const;
    virtual std::string GetDialogArgs() const;
    virtual void OnDialogClosed(const std::string& json_retval);
    virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
    }
    virtual bool ShouldShowDialogTitle() const { return true; }
    virtual bool HandleContextMenu(const ContextMenuParams& params) {
      return true;
    }

    DISALLOW_COPY_AND_ASSIGN(FileBrowseDelegate);
  };

  class FileBrowseDelegateHandler : public WebUIMessageHandler {
   public:
    explicit FileBrowseDelegateHandler(FileBrowseDelegate* delegate);

    // WebUIMessageHandler implementation.
    virtual void RegisterMessages();

    // Callback for the "setDialogTitle" message.
    void HandleSetDialogTitle(const ListValue* args);

   private:
    FileBrowseDelegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(FileBrowseDelegateHandler);
  };

  // Notification from FileBrowseDelegate when file browse UI is dismissed.
  void OnDialogClosed(FileBrowseDelegate* delegate, const std::string& json);

  // Callback method to open HTML
  void OpenHtmlDialog(gfx::NativeWindow owning_window,
                      FileBrowseDelegate* file_browse_delegate);

  // The set of all parent windows for which we are currently running dialogs.
  std::set<gfx::NativeWindow> parents_;

  // The set of all FileBrowseDelegate that we are currently running.
  std::set<FileBrowseDelegate*> delegates_;

  // True when opening in browser, otherwise in OOBE/login mode.
  bool browser_mode_;

  // The listener to be notified of selection completion.
  Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return new SelectFileDialogImpl(listener);
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : browser_mode_(true),
      listener_(listener) {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // All dialogs should be dismissed by now.
  DCHECK(parents_.empty() && delegates_.empty());
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parent_window && parents_.find(parent_window) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::SelectFile(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  std::wstring title_string;
  if (title.empty()) {
    int string_id;
    switch (type) {
      case SELECT_FOLDER:
        string_id = IDS_SELECT_FOLDER_DIALOG_TITLE;
        break;
      case SELECT_OPEN_FILE:
      case SELECT_OPEN_MULTI_FILE:
        string_id = IDS_OPEN_FILE_DIALOG_TITLE;
        break;
      case SELECT_SAVEAS_FILE:
        string_id = IDS_SAVE_AS_DIALOG_TITLE;
        break;
      default:
        NOTREACHED();
        return;
    }
    title_string = UTF16ToWide(l10n_util::GetStringUTF16(string_id));
  } else {
    title_string = UTF16ToWide(title);
  }

  if (owning_window)
    parents_.insert(owning_window);

  FileBrowseDelegate* file_browse_delegate = new FileBrowseDelegate(this,
      type, title_string, default_path, file_types, file_type_index,
      default_extension, owning_window, params);
  delegates_.insert(file_browse_delegate);

  if (browser_mode_) {
    Browser* browser = BrowserList::GetLastActive();
    DCHECK(browser);
    browser->BrowserShowHtmlDialog(file_browse_delegate, owning_window);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &SelectFileDialogImpl::OpenHtmlDialog,
                          owning_window,
                          file_browse_delegate));
  }
}

void SelectFileDialogImpl::OnDialogClosed(FileBrowseDelegate* delegate,
                                          const std::string& json) {
  // Nothing to do if listener_ is gone.

  if (!listener_)
    return;

  bool notification_fired = false;

  if (!json.empty()) {
    scoped_ptr<Value> value(base::JSONReader::Read(json, false));
    if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
      // Bad json value returned.
      NOTREACHED();
    } else {
      const DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
      if (delegate->type_ == SELECT_OPEN_FILE ||
          delegate->type_ == SELECT_SAVEAS_FILE ||
          delegate->type_ == SELECT_FOLDER) {
        std::string path_string;
        if (dict->HasKey(kKeyNamePath) &&
            dict->GetString(kKeyNamePath, &path_string)) {
#if defined(OS_WIN)
          FilePath path(base::SysUTF8ToWide(path_string));
#else
          FilePath path(
              base::SysWideToNativeMB(base::SysUTF8ToWide(path_string)));
#endif
          listener_->FileSelected(path, kSaveCompletePageIndex,
                                  delegate->params_);
          notification_fired = true;
        }
      } else if (delegate->type_ == SELECT_OPEN_MULTI_FILE) {
        ListValue* paths_value = NULL;
        if (dict->HasKey(kKeyNamePath) &&
            dict->GetList(kKeyNamePath, &paths_value) &&
            paths_value) {
          std::vector<FilePath> paths;
          paths.reserve(paths_value->GetSize());
          for (size_t i = 0; i < paths_value->GetSize(); ++i) {
            std::string path_string;
            if (paths_value->GetString(i, &path_string) &&
                !path_string.empty()) {
#if defined(OS_WIN)
              FilePath path(base::SysUTF8ToWide(path_string));
#else
              FilePath path(
                  base::SysWideToNativeMB(base::SysUTF8ToWide(path_string)));
#endif
              paths.push_back(path);
            }
          }

          listener_->MultiFilesSelected(paths, delegate->params_);
          notification_fired = true;
        }
      } else {
        NOTREACHED();
      }
    }
  }

  // Always notify listener when dialog is dismissed.
  if (!notification_fired)
    listener_->FileSelectionCanceled(delegate->params_);

  parents_.erase(delegate->parent_);
  delegates_.erase(delegate);
}

void SelectFileDialogImpl::OpenHtmlDialog(
    gfx::NativeWindow owning_window,
    FileBrowseDelegate* file_browse_delegate) {
  browser::ShowHtmlDialog(owning_window,
                          ProfileManager::GetDefaultProfile(),
                          file_browse_delegate);
}

SelectFileDialogImpl::FileBrowseDelegate::FileBrowseDelegate(
    SelectFileDialogImpl* owner,
    Type type,
    const std::wstring& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow parent,
    void* params)
  : owner_(owner),
    parent_(parent),
    type_(type),
    title_(title),
    default_path_(default_path),
    file_type_index_(file_type_index),
    default_extension_(default_extension),
    params_(params) {
  if (file_types)
    file_types_ = *file_types;
  else
    file_types_.include_all_files = true;
}

SelectFileDialogImpl::FileBrowseDelegate::~FileBrowseDelegate() {
}

bool SelectFileDialogImpl::FileBrowseDelegate::IsDialogModal() const {
  return true;
}

std::wstring SelectFileDialogImpl::FileBrowseDelegate::GetDialogTitle() const {
  return title_;
}

GURL SelectFileDialogImpl::FileBrowseDelegate::GetDialogContentURL() const {
  std::string url_string(chrome::kChromeUIFileBrowseURL);

  return GURL(url_string);
}

void SelectFileDialogImpl::FileBrowseDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(new FileBrowseDelegateHandler(
      const_cast<FileBrowseDelegate*>(this)));
  return;
}

void SelectFileDialogImpl::FileBrowseDelegate::GetDialogSize(
    gfx::Size* size) const {
  size->SetSize(320, 240);
}

std::string SelectFileDialogImpl::FileBrowseDelegate::GetDialogArgs() const {
  // SelectFile inputs as json.
  //   {
  //     "type"            : "open",   // (or "open_multiple", "save", "folder"
  //     "all_files"       : true,
  //     "file_types"      : {
  //                           "exts" : [ ["htm", "html"], ["txt"] ],
  //                           "desc" : [ "HTML files", "Text files" ],
  //                         },
  //     "file_type_index" : 1,    // 1-based file type index.
  //   }
  // See browser/ui/shell_dialogs.h for more details.

  std::string type_string;
  switch (type_) {
    case SELECT_FOLDER:
      type_string = "folder";
      break;
    case SELECT_OPEN_FILE:
      type_string = "open";
      break;
    case SELECT_OPEN_MULTI_FILE:
      type_string = "open_multiple";
      break;
    case SELECT_SAVEAS_FILE:
      type_string = "save";
      break;
    default:
      NOTREACHED();
      return std::string();
  }

  std::string exts_list;
  std::string desc_list;
  for (size_t i = 0; i < file_types_.extensions.size(); ++i) {
    DCHECK(!file_types_.extensions[i].empty());

    std::string exts;
    for (size_t j = 0; j < file_types_.extensions[i].size(); ++j) {
      if (!exts.empty())
        exts.append(",");
      base::StringAppendF(&exts, "\"%s\"",
                          file_types_.extensions[i][j].c_str());
    }

    if (!exts_list.empty())
      exts_list.append(",");
    base::StringAppendF(&exts_list, "[%s]", exts.c_str());

    std::string desc;
    if (i < file_types_.extension_description_overrides.size()) {
      desc = UTF16ToUTF8(file_types_.extension_description_overrides[i]);
    } else {
#if defined(OS_WIN)
      desc = WideToUTF8(file_types_.extensions[i][0]);
#elif defined(OS_POSIX)
      desc = file_types_.extensions[i][0];
#else
      NOTIMPLEMENTED();
#endif
    }

    if (!desc_list.empty())
      desc_list.append(",");
    base::StringAppendF(&desc_list, "\"%s\"", desc.c_str());
  }

  std::string filename = default_path_.BaseName().value();

  return StringPrintf("{"
        "\"type\":\"%s\","
        "\"all_files\":%s,"
        "\"current_file\":\"%s\","
        "\"file_types\":{\"exts\":[%s],\"desc\":[%s]},"
        "\"file_type_index\":%d"
      "}",
      type_string.c_str(),
      file_types_.include_all_files ? "true" : "false",
      filename.c_str(),
      exts_list.c_str(),
      desc_list.c_str(),
      file_type_index_);
}

void SelectFileDialogImpl::FileBrowseDelegate::OnDialogClosed(
    const std::string& json_retval) {
  owner_->OnDialogClosed(this, json_retval);
  delete this;
  return;
}

SelectFileDialogImpl::FileBrowseDelegateHandler::FileBrowseDelegateHandler(
    FileBrowseDelegate* delegate)
    : delegate_(delegate) {
}

void SelectFileDialogImpl::FileBrowseDelegateHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("setDialogTitle",
      NewCallback(this, &FileBrowseDelegateHandler::HandleSetDialogTitle));
}

void SelectFileDialogImpl::FileBrowseDelegateHandler::HandleSetDialogTitle(
    const ListValue* args) {
  std::wstring new_title = ExtractStringValue(args);
  if (new_title != delegate_->title_) {
    delegate_->title_ = new_title;

    // Notify the containing view about the title change.
    // The current HtmlDialogUIDelegate and HtmlDialogView does not support
    // dynamic title change. We hijacked the mechanism between HTMLDialogUI
    // and HtmlDialogView to get the HtmlDialgoView and forced it to update
    // its title.
    // TODO(xiyuan): Change this when the infrastructure is improved.
    HtmlDialogUIDelegate** delegate = HtmlDialogUI::GetPropertyAccessor().
        GetProperty(web_ui_->tab_contents()->property_bag());
    HtmlDialogView* containing_view = static_cast<HtmlDialogView*>(*delegate);
    DCHECK(containing_view);

    containing_view->GetWindow()->UpdateWindowTitle();
    containing_view->GetWindow()->GetNonClientView()->SchedulePaint();
  }
}
