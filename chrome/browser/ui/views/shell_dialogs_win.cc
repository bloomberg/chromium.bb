// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shell_dialogs.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <set>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_comptr_win.h"
#include "base/string_split.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "content/browser/browser_thread.h"
#include "grit/app_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"

// This function takes the output of a SaveAs dialog: a filename, a filter and
// the extension originally suggested to the user (shown in the dialog box) and
// returns back the filename with the appropriate extension tacked on. If the
// user requests an unknown extension and is not using the 'All files' filter,
// the suggested extension will be appended, otherwise we will leave the
// filename unmodified. |filename| should contain the filename selected in the
// SaveAs dialog box and may include the path, |filter_selected| should be
// '*.something', for example '*.*' or it can be blank (which is treated as
// *.*). |suggested_ext| should contain the extension without the dot (.) in
// front, for example 'jpg'.
std::wstring AppendExtensionIfNeeded(const std::wstring& filename,
                                     const std::wstring& filter_selected,
                                     const std::wstring& suggested_ext) {
  DCHECK(!filename.empty());
  std::wstring return_value = filename;

  // If we wanted a specific extension, but the user's filename deleted it or
  // changed it to something that the system doesn't understand, re-append.
  // Careful: Checking net::GetMimeTypeFromExtension() will only find
  // extensions with a known MIME type, which many "known" extensions on Windows
  // don't have.  So we check directly for the "known extension" registry key.
  std::wstring file_extension(file_util::GetFileExtensionFromPath(filename));
  std::wstring key(L"." + file_extension);
  if (!(filter_selected.empty() || filter_selected == L"*.*") &&
      !base::win::RegKey(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ).Valid() &&
      file_extension != suggested_ext) {
    if (return_value[return_value.length() - 1] != L'.')
      return_value.append(L".");
    return_value.append(suggested_ext);
  }

  // Strip any trailing dots, which Windows doesn't allow.
  size_t index = return_value.find_last_not_of(L'.');
  if (index < return_value.size() - 1)
    return_value.resize(index + 1);

  return return_value;
}

namespace {

// Get the file type description from the registry. This will be "Text Document"
// for .txt files, "JPEG Image" for .jpg files, etc. If the registry doesn't
// have an entry for the file type, we return false, true if the description was
// found. 'file_ext' must be in form ".txt".
static bool GetRegistryDescriptionFromExtension(const std::wstring& file_ext,
                                                std::wstring* reg_description) {
  DCHECK(reg_description);
  base::win::RegKey reg_ext(HKEY_CLASSES_ROOT, file_ext.c_str(), KEY_READ);
  std::wstring reg_app;
  if (reg_ext.ReadValue(NULL, &reg_app) == ERROR_SUCCESS && !reg_app.empty()) {
    base::win::RegKey reg_link(HKEY_CLASSES_ROOT, reg_app.c_str(), KEY_READ);
    if (reg_link.ReadValue(NULL, reg_description) == ERROR_SUCCESS)
      return true;
  }
  return false;
}

// Set up a filter for a Save/Open dialog, which will consist of |file_ext| file
// extensions (internally separated by semicolons), |ext_desc| as the text
// descriptions of the |file_ext| types (optional), and (optionally) the default
// 'All Files' view. The purpose of the filter is to show only files of a
// particular type in a Windows Save/Open dialog box. The resulting filter is
// returned. The filters created here are:
//   1. only files that have 'file_ext' as their extension
//   2. all files (only added if 'include_all_files' is true)
// Example:
//   file_ext: { "*.txt", "*.htm;*.html" }
//   ext_desc: { "Text Document" }
//   returned: "Text Document\0*.txt\0HTML Document\0*.htm;*.html\0"
//             "All Files\0*.*\0\0" (in one big string)
// If a description is not provided for a file extension, it will be retrieved
// from the registry. If the file extension does not exist in the registry, it
// will be omitted from the filter, as it is likely a bogus extension.
std::wstring FormatFilterForExtensions(
    const std::vector<std::wstring>& file_ext,
    const std::vector<std::wstring>& ext_desc,
    bool include_all_files) {
  const std::wstring all_ext = L"*.*";
  const std::wstring all_desc =
      l10n_util::GetStringUTF16(IDS_APP_SAVEAS_ALL_FILES);

  DCHECK(file_ext.size() >= ext_desc.size());

  std::wstring result;

  for (size_t i = 0; i < file_ext.size(); ++i) {
    std::wstring ext = file_ext[i];
    std::wstring desc;
    if (i < ext_desc.size())
      desc = ext_desc[i];

    if (ext.empty()) {
      // Force something reasonable to appear in the dialog box if there is no
      // extension provided.
      include_all_files = true;
      continue;
    }

    if (desc.empty()) {
      DCHECK(ext.find(L'.') != std::wstring::npos);
      std::wstring first_extension = ext.substr(ext.find(L'.'));
      size_t first_separator_index = first_extension.find(L';');
      if (first_separator_index != std::wstring::npos)
        first_extension = first_extension.substr(0, first_separator_index);

      // Find the extension name without the preceeding '.' character.
      std::wstring ext_name = first_extension;
      size_t ext_index = ext_name.find_first_not_of(L'.');
      if (ext_index != std::wstring::npos)
        ext_name = ext_name.substr(ext_index);

      if (!GetRegistryDescriptionFromExtension(first_extension, &desc)) {
        // The extension doesn't exist in the registry. Create a description
        // based on the unknown extension type (i.e. if the extension is .qqq,
        // the we create a description "QQQ File (.qqq)").
        include_all_files = true;
        desc = l10n_util::GetStringFUTF16(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                          l10n_util::ToUpper(ext_name),
                                          ext_name);
      }
      if (desc.empty())
        desc = L"*." + ext_name;
    }

    result.append(desc.c_str(), desc.size() + 1);  // Append NULL too.
    result.append(ext.c_str(), ext.size() + 1);
  }

  if (include_all_files) {
    result.append(all_desc.c_str(), all_desc.size() + 1);
    result.append(all_ext.c_str(), all_ext.size() + 1);
  }

  result.append(1, '\0');  // Double NULL required.
  return result;
}

// Enforce visible dialog box.
UINT_PTR CALLBACK SaveAsDialogHook(HWND dialog, UINT message,
                                   WPARAM wparam, LPARAM lparam) {
  static const UINT kPrivateMessage = 0x2F3F;
  switch (message) {
    case WM_INITDIALOG: {
      // Do nothing here. Just post a message to defer actual processing.
      PostMessage(dialog, kPrivateMessage, 0, 0);
      return TRUE;
    }
    case kPrivateMessage: {
      // The dialog box is the parent of the current handle.
      HWND real_dialog = GetParent(dialog);

      // Retrieve the final size.
      RECT dialog_rect;
      GetWindowRect(real_dialog, &dialog_rect);

      // Verify that the upper left corner is visible.
      POINT point = { dialog_rect.left, dialog_rect.top };
      HMONITOR monitor1 = MonitorFromPoint(point, MONITOR_DEFAULTTONULL);
      point.x = dialog_rect.right;
      point.y = dialog_rect.bottom;

      // Verify that the lower right corner is visible.
      HMONITOR monitor2 = MonitorFromPoint(point, MONITOR_DEFAULTTONULL);
      if (monitor1 && monitor2)
        return 0;

      // Some part of the dialog box is not visible, fix it by moving is to the
      // client rect position of the browser window.
      HWND parent_window = GetParent(real_dialog);
      if (!parent_window)
        return 0;
      WINDOWINFO parent_info;
      parent_info.cbSize = sizeof(WINDOWINFO);
      GetWindowInfo(parent_window, &parent_info);
      SetWindowPos(real_dialog, NULL,
                   parent_info.rcClient.left,
                   parent_info.rcClient.top,
                   0, 0,  // Size.
                   SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE |
                   SWP_NOZORDER);

      return 0;
    }
  }
  return 0;
}

// Prompt the user for location to save a file.
// Callers should provide the filter string, and also a filter index.
// The parameter |index| indicates the initial index of filter description
// and filter pattern for the dialog box. If |index| is zero or greater than
// the number of total filter types, the system uses the first filter in the
// |filter| buffer. |index| is used to specify the initial selected extension,
// and when done contains the extension the user chose. The parameter
// |final_name| returns the file name which contains the drive designator,
// path, file name, and extension of the user selected file name. |def_ext| is
// the default extension to give to the file if the user did not enter an
// extension. If |ignore_suggested_ext| is true, any file extension contained in
// |suggested_name| will not be used to generate the file name. This is useful
// in the case of saving web pages, where we know the extension type already and
// where |suggested_name| may contain a '.' character as a valid part of the
// name, thus confusing our extension detection code.
bool SaveFileAsWithFilter(HWND owner,
                          const std::wstring& suggested_name,
                          const std::wstring& filter,
                          const std::wstring& def_ext,
                          bool ignore_suggested_ext,
                          unsigned* index,
                          std::wstring* final_name) {
  DCHECK(final_name);
  // Having an empty filter makes for a bad user experience. We should always
  // specify a filter when saving.
  DCHECK(!filter.empty());
  std::wstring file_part = FilePath(suggested_name).BaseName().value();

  // The size of the in/out buffer in number of characters we pass to win32
  // GetSaveFileName.  From MSDN "The buffer must be large enough to store the
  // path and file name string or strings, including the terminating NULL
  // character.  ... The buffer should be at least 256 characters long.".
  // _IsValidPathComDlg does a copy expecting at most MAX_PATH, otherwise will
  // result in an error of FNERR_INVALIDFILENAME.  So we should only pass the
  // API a buffer of at most MAX_PATH.
  wchar_t file_name[MAX_PATH];
  base::wcslcpy(file_name, file_part.c_str(), arraysize(file_name));

  OPENFILENAME save_as;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&save_as, sizeof(save_as));
  save_as.lStructSize = sizeof(OPENFILENAME);
  save_as.hwndOwner = owner;
  save_as.hInstance = NULL;

  save_as.lpstrFilter = filter.empty() ? NULL : filter.c_str();

  save_as.lpstrCustomFilter = NULL;
  save_as.nMaxCustFilter = 0;
  save_as.nFilterIndex = *index;
  save_as.lpstrFile = file_name;
  save_as.nMaxFile = arraysize(file_name);
  save_as.lpstrFileTitle = NULL;
  save_as.nMaxFileTitle = 0;

  // Set up the initial directory for the dialog.
  std::wstring directory;
  if (!suggested_name.empty())
     directory = FilePath(suggested_name).DirName().value();

  save_as.lpstrInitialDir = directory.c_str();
  save_as.lpstrTitle = NULL;
  save_as.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
                  OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
  save_as.lpstrDefExt = &def_ext[0];
  save_as.lCustData = NULL;

  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    // The save as on Windows XP remembers its last position,
    // and if the screen resolution changed, it will be off screen.
    save_as.Flags |= OFN_ENABLEHOOK;
    save_as.lpfnHook = &SaveAsDialogHook;
  }

  // Must be NULL or 0.
  save_as.pvReserved = NULL;
  save_as.dwReserved = 0;

  if (!GetSaveFileName(&save_as)) {
    // Zero means the dialog was closed, otherwise we had an error.
    DWORD error_code = CommDlgExtendedError();
    if (error_code != 0) {
      NOTREACHED() << "GetSaveFileName failed with code: " << error_code;
    }
    return false;
  }

  // Return the user's choice.
  final_name->assign(save_as.lpstrFile);
  *index = save_as.nFilterIndex;

  // Figure out what filter got selected from the vector with embedded nulls.
  // NOTE: The filter contains a string with embedded nulls, such as:
  // JPG Image\0*.jpg\0All files\0*.*\0\0
  // The filter index is 1-based index for which pair got selected. So, using
  // the example above, if the first index was selected we need to skip 1
  // instance of null to get to "*.jpg".
  std::vector<std::wstring> filters;
  if (!filter.empty() && save_as.nFilterIndex > 0)
    base::SplitString(filter, '\0', &filters);
  std::wstring filter_selected;
  if (!filters.empty())
    filter_selected = filters[(2 * (save_as.nFilterIndex - 1)) + 1];

  // Get the extension that was suggested to the user (when the Save As dialog
  // was opened). For saving web pages, we skip this step since there may be
  // 'extension characters' in the title of the web page.
  std::wstring suggested_ext;
  if (!ignore_suggested_ext)
    suggested_ext = file_util::GetFileExtensionFromPath(suggested_name);

  // If we can't get the extension from the suggested_name, we use the default
  // extension passed in. This is to cover cases like when saving a web page,
  // where we get passed in a name without an extension and a default extension
  // along with it.
  if (suggested_ext.empty())
    suggested_ext = def_ext;

  *final_name =
      AppendExtensionIfNeeded(*final_name, filter_selected, suggested_ext);
  return true;
}

// Prompt the user for location to save a file. 'suggested_name' is a full path
// that gives the dialog box a hint as to how to initialize itself.
// For example, a 'suggested_name' of:
//   "C:\Documents and Settings\jojo\My Documents\picture.png"
// will start the dialog in the "C:\Documents and Settings\jojo\My Documents\"
// directory, and filter for .png file types.
// 'owner' is the window to which the dialog box is modal, NULL for a modeless
// dialog box.
// On success,  returns true and 'final_name' contains the full path of the file
// that the user chose. On error, returns false, and 'final_name' is not
// modified.
bool SaveFileAs(HWND owner,
                const std::wstring& suggested_name,
                std::wstring* final_name) {
  std::wstring file_ext = FilePath(suggested_name).Extension().insert(0, L"*");
  std::wstring filter = FormatFilterForExtensions(
      std::vector<std::wstring>(1, file_ext),
      std::vector<std::wstring>(),
      true);
  unsigned index = 1;
  return SaveFileAsWithFilter(owner,
                              suggested_name,
                              filter,
                              L"",
                              false,
                              &index,
                              final_name);
}

}  // namespace

// Helpers to show certain types of Windows shell dialogs in a way that doesn't
// block the UI of the entire app.

class ShellDialogThread : public base::Thread {
 public:
  ShellDialogThread() : base::Thread("Chrome_ShellDialogThread") { }

 protected:
  void Init() {
    // Initializes the COM library on the current thread.
    CoInitialize(NULL);
  }

  void CleanUp() {
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDialogThread);
};

///////////////////////////////////////////////////////////////////////////////
// A base class for all shell dialog implementations that handles showing a
// shell dialog modally on its own thread.
class BaseShellDialogImpl {
 public:
  BaseShellDialogImpl();
  virtual ~BaseShellDialogImpl();

 protected:
  // Represents a run of a dialog.
  struct RunState {
    // Owning HWND, may be null.
    HWND owner;

    // Thread dialog is run on.
    base::Thread* dialog_thread;
  };

  // Called at the beginning of a modal dialog run. Disables the owner window
  // and tracks it. Returns the message loop of the thread that the dialog will
  // be run on.
  RunState BeginRun(HWND owner);

  // Cleans up after a dialog run. If the run_state has a valid HWND this makes
  // sure that the window is enabled. This is essential because BeginRun
  // aggressively guards against multiple modal dialogs per HWND. Must be called
  // on the UI thread after the result of the dialog has been determined.
  //
  // In addition this deletes the Thread in RunState.
  void EndRun(RunState run_state);

  // Returns true if a modal shell dialog is currently active for the specified
  // owner. Must be called on the UI thread.
  bool IsRunningDialogForOwner(HWND owner) const;

  // Disables the window |owner|. Can be run from either the ui or the dialog
  // thread. Can be called on either the UI or the dialog thread. This function
  // is called on the dialog thread after the modal Windows Common dialog
  // functions return because Windows automatically re-enables the owning
  // window when those functions return, but we don't actually want them to be
  // re-enabled until the response of the dialog propagates back to the UI
  // thread, so we disable the owner manually after the Common dialog function
  // returns.
  void DisableOwner(HWND owner);

 private:
  // Creates a thread to run a shell dialog on. Each dialog requires its own
  // thread otherwise in some situations where a singleton owns a single
  // instance of this object we can have a situation where a modal dialog in
  // one window blocks the appearance of a modal dialog in another.
  static base::Thread* CreateDialogThread();

  // Enables the window |owner_|. Can only be run from the ui thread.
  void EnableOwner(HWND owner);

  // A list of windows that currently own active shell dialogs for this
  // instance. For example, if the DownloadManager owns an instance of this
  // object and there are two browser windows open both with Save As dialog
  // boxes active, this list will consist of the two browser windows' HWNDs.
  // The derived class must call EndRun once the dialog is done showing to
  // remove the owning HWND from this list.
  // This object is static since it is maintained for all instances of this
  // object - i.e. you can't have a font picker and a file picker open for the
  // same owner, even though they might be represented by different instances
  // of this object.
  // This set only contains non-null HWNDs. NULL hwnds are not added to this
  // list.
  typedef std::set<HWND> Owners;
  static Owners owners_;
  static int instance_count_;

  DISALLOW_COPY_AND_ASSIGN(BaseShellDialogImpl);
};

// static
BaseShellDialogImpl::Owners BaseShellDialogImpl::owners_;
int BaseShellDialogImpl::instance_count_ = 0;

BaseShellDialogImpl::BaseShellDialogImpl() {
  ++instance_count_;
}

BaseShellDialogImpl::~BaseShellDialogImpl() {
  // All runs should be complete by the time this is called!
  if (--instance_count_ == 0)
    DCHECK(owners_.empty());
}

BaseShellDialogImpl::RunState BaseShellDialogImpl::BeginRun(HWND owner) {
  // Cannot run a modal shell dialog if one is already running for this owner.
  DCHECK(!IsRunningDialogForOwner(owner));
  // The owner must be a top level window, otherwise we could end up with two
  // entries in our map for the same top level window.
  DCHECK(!owner || owner == GetAncestor(owner, GA_ROOT));
  RunState run_state;
  run_state.dialog_thread = CreateDialogThread();
  run_state.owner = owner;
  if (owner) {
    owners_.insert(owner);
    DisableOwner(owner);
  }
  return run_state;
}

void BaseShellDialogImpl::EndRun(RunState run_state) {
  if (run_state.owner) {
    DCHECK(IsRunningDialogForOwner(run_state.owner));
    EnableOwner(run_state.owner);
    DCHECK(owners_.find(run_state.owner) != owners_.end());
    owners_.erase(run_state.owner);
  }
  DCHECK(run_state.dialog_thread);
  delete run_state.dialog_thread;
}

bool BaseShellDialogImpl::IsRunningDialogForOwner(HWND owner) const {
  return (owner && owners_.find(owner) != owners_.end());
}

void BaseShellDialogImpl::DisableOwner(HWND owner) {
  if (IsWindow(owner))
    EnableWindow(owner, FALSE);
}

// static
base::Thread* BaseShellDialogImpl::CreateDialogThread() {
  base::Thread* thread = new ShellDialogThread;
  bool started = thread->Start();
  DCHECK(started);
  return thread;
}

void BaseShellDialogImpl::EnableOwner(HWND owner) {
  if (IsWindow(owner))
    EnableWindow(owner, TRUE);
}

// Implementation of SelectFileDialog that shows a Windows common dialog for
// choosing a file or folder.
class SelectFileDialogImpl : public SelectFileDialog,
                             public BaseShellDialogImpl {
 public:
  explicit SelectFileDialogImpl(Listener* listener);

  // SelectFileDialog implementation:
  virtual void SelectFile(Type type,
                          const string16& title,
                          const FilePath& default_path,
                          const FileTypeInfo* file_types,
                          int file_type_index,
                          const FilePath::StringType& default_extension,
                          gfx::NativeWindow owning_window,
                          void* params);
  virtual bool IsRunning(HWND owning_hwnd) const;
  virtual void ListenerDestroyed();

 private:
  virtual ~SelectFileDialogImpl();

  // A struct for holding all the state necessary for displaying a Save dialog.
  struct ExecuteSelectParams {
    ExecuteSelectParams(Type type,
                        const std::wstring& title,
                        const FilePath& default_path,
                        const FileTypeInfo* file_types,
                        int file_type_index,
                        const std::wstring& default_extension,
                        RunState run_state,
                        HWND owner,
                        void* params)
        : type(type),
          title(title),
          default_path(default_path),
          file_type_index(file_type_index),
          default_extension(default_extension),
          run_state(run_state),
          owner(owner),
          params(params) {
      if (file_types) {
        this->file_types = *file_types;
      } else {
        this->file_types.include_all_files = true;
      }
    }
    SelectFileDialog::Type type;
    std::wstring title;
    FilePath default_path;
    FileTypeInfo file_types;
    int file_type_index;
    std::wstring default_extension;
    RunState run_state;
    HWND owner;
    void* params;
  };

  // Shows the file selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFile(const ExecuteSelectParams& params);

  // Notifies the listener that a folder was chosen. Run on the ui thread.
  void FileSelected(const FilePath& path, int index,
                    void* params, RunState run_state);

  // Notifies listener that multiple files were chosen. Run on the ui thread.
  void MultiFilesSelected(const std::vector<FilePath>& paths, void* params,
                         RunState run_state);

  // Notifies the listener that no file was chosen (the action was canceled).
  // Run on the ui thread.
  void FileNotSelected(void* params, RunState run_state);

  // Runs a Folder selection dialog box, passes back the selected folder in
  // |path| and returns true if the user clicks OK. If the user cancels the
  // dialog box the value in |path| is not modified and returns false. |title|
  // is the user-supplied title text to show for the dialog box. Run on the
  // dialog thread.
  bool RunSelectFolderDialog(const std::wstring& title,
                             HWND owner,
                             FilePath* path);

  // Runs an Open file dialog box, with similar semantics for input paramaters
  // as RunSelectFolderDialog.
  bool RunOpenFileDialog(const std::wstring& title,
                         const std::wstring& filters,
                         HWND owner,
                         FilePath* path);

  // Runs an Open file dialog box that supports multi-select, with similar
  // semantics for input paramaters as RunOpenFileDialog.
  bool RunOpenMultiFileDialog(const std::wstring& title,
                              const std::wstring& filter,
                              HWND owner,
                              std::vector<FilePath>* paths);

  // The callback function for when the select folder dialog is opened.
  static int CALLBACK BrowseCallbackProc(HWND window, UINT message,
                                         LPARAM parameter,
                                         LPARAM data);

  // The listener to be notified of selection completion.
  Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : listener_(listener),
      BaseShellDialogImpl() {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
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
  ExecuteSelectParams execute_params(type, UTF16ToWide(title), default_path,
                                     file_types, file_type_index,
                                     default_extension, BeginRun(owning_window),
                                     owning_window, params);
  execute_params.run_state.dialog_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SelectFileDialogImpl::ExecuteSelectFile,
                        execute_params));
}

bool SelectFileDialogImpl::IsRunning(HWND owning_hwnd) const {
  return listener_ && IsRunningDialogForOwner(owning_hwnd);
}

void SelectFileDialogImpl::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = NULL;
}

void SelectFileDialogImpl::ExecuteSelectFile(
    const ExecuteSelectParams& params) {
  std::vector<std::wstring> exts;
  for (size_t i = 0; i < params.file_types.extensions.size(); ++i) {
    const std::vector<std::wstring>& inner_exts =
        params.file_types.extensions[i];
    std::wstring ext_string;
    for (size_t j = 0; j < inner_exts.size(); ++j) {
      if (!ext_string.empty())
        ext_string.push_back(L';');
      ext_string.append(L"*.");
      ext_string.append(inner_exts[j]);
    }
    exts.push_back(ext_string);
  }
  std::wstring filter = FormatFilterForExtensions(
      exts,
      params.file_types.extension_description_overrides,
      params.file_types.include_all_files);

  FilePath path = params.default_path;
  bool success = false;
  unsigned filter_index = params.file_type_index;
  if (params.type == SELECT_FOLDER) {
    success = RunSelectFolderDialog(params.title,
                                    params.run_state.owner,
                                    &path);
  } else if (params.type == SELECT_SAVEAS_FILE) {
    std::wstring path_as_wstring = path.value();
    success = SaveFileAsWithFilter(params.run_state.owner,
        params.default_path.value(), filter,
        params.default_extension, false, &filter_index, &path_as_wstring);
    if (success)
      path = FilePath(path_as_wstring);
    DisableOwner(params.run_state.owner);
  } else if (params.type == SELECT_OPEN_FILE) {
    success = RunOpenFileDialog(params.title, filter,
                                params.run_state.owner, &path);
  } else if (params.type == SELECT_OPEN_MULTI_FILE) {
    std::vector<FilePath> paths;
    if (RunOpenMultiFileDialog(params.title, filter,
                               params.run_state.owner, &paths)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(
              this, &SelectFileDialogImpl::MultiFilesSelected, paths,
              params.params, params.run_state));
      return;
    }
  }

  if (success) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &SelectFileDialogImpl::FileSelected, path, filter_index,
            params.params, params.run_state));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &SelectFileDialogImpl::FileNotSelected, params.params,
            params.run_state));
  }
}

void SelectFileDialogImpl::FileSelected(const FilePath& selected_folder,
                                        int index,
                                        void* params,
                                        RunState run_state) {
  if (listener_)
    listener_->FileSelected(selected_folder, index, params);
  EndRun(run_state);
}

void SelectFileDialogImpl::MultiFilesSelected(
    const std::vector<FilePath>& selected_files,
    void* params,
    RunState run_state) {
  if (listener_)
    listener_->MultiFilesSelected(selected_files, params);
  EndRun(run_state);
}

void SelectFileDialogImpl::FileNotSelected(void* params, RunState run_state) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
  EndRun(run_state);
}

int CALLBACK SelectFileDialogImpl::BrowseCallbackProc(HWND window,
                                                      UINT message,
                                                      LPARAM parameter,
                                                      LPARAM data) {
  if (message == BFFM_INITIALIZED) {
    // WParam is TRUE since passing a path.
    // data lParam member of the BROWSEINFO structure.
    SendMessage(window, BFFM_SETSELECTION, TRUE, (LPARAM)data);
  }
  return 0;
}

bool SelectFileDialogImpl::RunSelectFolderDialog(const std::wstring& title,
                                                 HWND owner,
                                                 FilePath* path) {
  DCHECK(path);

  wchar_t dir_buffer[MAX_PATH + 1];

  bool result = false;
  BROWSEINFO browse_info = {0};
  browse_info.hwndOwner = owner;
  browse_info.lpszTitle = title.c_str();
  browse_info.pszDisplayName = dir_buffer;
  browse_info.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;

  if (path->value().length()) {
    // Highlight the current value.
    browse_info.lParam = (LPARAM)path->value().c_str();
    browse_info.lpfn = &BrowseCallbackProc;
  }

  LPITEMIDLIST list = SHBrowseForFolder(&browse_info);
  DisableOwner(owner);
  if (list) {
    STRRET out_dir_buffer;
    ZeroMemory(&out_dir_buffer, sizeof(out_dir_buffer));
    out_dir_buffer.uType = STRRET_WSTR;
    ScopedComPtr<IShellFolder> shell_folder;
    if (SHGetDesktopFolder(shell_folder.Receive()) == NOERROR) {
      HRESULT hr = shell_folder->GetDisplayNameOf(list, SHGDN_FORPARSING,
                                                  &out_dir_buffer);
      if (SUCCEEDED(hr) && out_dir_buffer.uType == STRRET_WSTR) {
        *path = FilePath(out_dir_buffer.pOleStr);
        CoTaskMemFree(out_dir_buffer.pOleStr);
        result = true;
      } else {
        // Use old way if we don't get what we want.
        wchar_t old_out_dir_buffer[MAX_PATH + 1];
        if (SHGetPathFromIDList(list, old_out_dir_buffer)) {
          *path = FilePath(old_out_dir_buffer);
          result = true;
        }
      }

      // According to MSDN, win2000 will not resolve shortcuts, so we do it
      // ourself.
      file_util::ResolveShortcut(path);
    }
    CoTaskMemFree(list);
  }
  return result;
}

bool SelectFileDialogImpl::RunOpenFileDialog(
    const std::wstring& title,
    const std::wstring& filter,
    HWND owner,
    FilePath* path) {
  OPENFILENAME ofn;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH];
  // According to http://support.microsoft.com/?scid=kb;en-us;222003&x=8&y=12,
  // The lpstrFile Buffer MUST be NULL Terminated.
  filename[0] = 0;
  // Define the dir in here to keep the string buffer pointer pointed to
  // ofn.lpstrInitialDir available during the period of running the
  // GetOpenFileName.
  FilePath dir;
  // Use lpstrInitialDir to specify the initial directory
  if (!path->empty()) {
    bool is_dir;
    base::PlatformFileInfo file_info;
    if (file_util::GetFileInfo(*path, &file_info))
      is_dir = file_info.is_directory;
    else
      is_dir = file_util::EndsWithSeparator(*path);
    if (is_dir) {
      ofn.lpstrInitialDir = path->value().c_str();
    } else {
      dir = path->DirName();
      ofn.lpstrInitialDir = dir.value().c_str();
      // Only pure filename can be put in lpstrFile field.
      base::wcslcpy(filename, path->BaseName().value().c_str(),
                    arraysize(filename));
    }
  }

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (!filter.empty())
    ofn.lpstrFilter = filter.c_str();
  bool success = !!GetOpenFileName(&ofn);
  DisableOwner(owner);
  if (success)
    *path = FilePath(filename);
  return success;
}

bool SelectFileDialogImpl::RunOpenMultiFileDialog(
    const std::wstring& title,
    const std::wstring& filter,
    HWND owner,
    std::vector<FilePath>* paths) {
  OPENFILENAME ofn;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  scoped_array<wchar_t> filename(new wchar_t[UNICODE_STRING_MAX_CHARS]);
  filename[0] = 0;

  ofn.lpstrFile = filename.get();
  ofn.nMaxFile = UNICODE_STRING_MAX_CHARS;
  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER
               | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT;

  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
  }
  bool success = !!GetOpenFileName(&ofn);
  DisableOwner(owner);
  if (success) {
    std::vector<FilePath> files;
    const wchar_t* selection = ofn.lpstrFile;
    while (*selection) {  // Empty string indicates end of list.
      files.push_back(FilePath(selection));
      // Skip over filename and null-terminator.
      selection += files.back().value().length() + 1;
    }
    if (files.empty()) {
      success = false;
    } else if (files.size() == 1) {
      // When there is one file, it contains the path and filename.
      paths->swap(files);
    } else {
      // Otherwise, the first string is the path, and the remainder are
      // filenames.
      std::vector<FilePath>::iterator path = files.begin();
      for (std::vector<FilePath>::iterator file = path + 1;
           file != files.end(); ++file) {
        paths->push_back(path->Append(*file));
      }
    }
  }
  return success;
}

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  return new SelectFileDialogImpl(listener);
}

///////////////////////////////////////////////////////////////////////////////
// SelectFontDialogImpl
//  Implementation of SelectFontDialog that shows a Windows common dialog for
//  choosing a font.
class SelectFontDialogImpl : public SelectFontDialog,
                             public BaseShellDialogImpl {
 public:
  explicit SelectFontDialogImpl(Listener* listener);

  // SelectFontDialog implementation:
  virtual void SelectFont(HWND owning_hwnd, void* params);
  virtual void SelectFont(HWND owning_hwnd,
                          void* params,
                          const std::wstring& font_name,
                          int font_size);
  virtual bool IsRunning(HWND owning_hwnd) const;
  virtual void ListenerDestroyed();

 private:
  virtual ~SelectFontDialogImpl();

  // Shows the font selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFont(RunState run_state, void* params);

  // Shows the font selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFontWithNameSize(RunState run_state,
                                     void* params,
                                     const std::wstring& font_name,
                                     int font_size);

  // Notifies the listener that a font was chosen. Run on the ui thread.
  void FontSelected(LOGFONT logfont, void* params, RunState run_state);

  // Notifies the listener that no font was chosen (the action was canceled).
  // Run on the ui thread.
  void FontNotSelected(void* params, RunState run_state);

  // The listener to be notified of selection completion.
  Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SelectFontDialogImpl);
};

SelectFontDialogImpl::SelectFontDialogImpl(Listener* listener)
    : listener_(listener),
      BaseShellDialogImpl() {
}

SelectFontDialogImpl::~SelectFontDialogImpl() {
}

void SelectFontDialogImpl::SelectFont(HWND owner, void* params) {
  RunState run_state = BeginRun(owner);
  run_state.dialog_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SelectFontDialogImpl::ExecuteSelectFont,
          run_state, params));
}

void SelectFontDialogImpl::SelectFont(HWND owner, void* params,
                                      const std::wstring& font_name,
                                      int font_size) {
  RunState run_state = BeginRun(owner);
  run_state.dialog_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this,
          &SelectFontDialogImpl::ExecuteSelectFontWithNameSize, run_state,
          params, font_name, font_size));
}

bool SelectFontDialogImpl::IsRunning(HWND owning_hwnd) const {
  return listener_ && IsRunningDialogForOwner(owning_hwnd);
}

void SelectFontDialogImpl::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = NULL;
}

void SelectFontDialogImpl::ExecuteSelectFont(RunState run_state, void* params) {
  LOGFONT logfont;
  CHOOSEFONT cf;
  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = run_state.owner;
  cf.lpLogFont = &logfont;
  cf.Flags = CF_SCREENFONTS;
  bool success = !!ChooseFont(&cf);
  DisableOwner(run_state.owner);
  if (success) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &SelectFontDialogImpl::FontSelected, logfont, params,
            run_state));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &SelectFontDialogImpl::FontNotSelected, params, run_state));
  }
}

void SelectFontDialogImpl::ExecuteSelectFontWithNameSize(
    RunState run_state, void* params, const std::wstring& font_name,
    int font_size) {
  // Create the HFONT from font name and size.
  HDC hdc = GetDC(NULL);
  long lf_height = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ReleaseDC(NULL, hdc);
  HFONT hf = ::CreateFont(lf_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          font_name.c_str());
  LOGFONT logfont;
  GetObject(hf, sizeof(LOGFONT), &logfont);
  // Retrieve the localized face name of the above font and update the LOGFONT
  // structure. When a font has a localized name matching to the system locale,
  // GetTextFace() returns the localized name. We should pass this localized
  // name to ChooseFont() so it can set the focus.
  HDC memory_dc = CreateCompatibleDC(NULL);
  if (memory_dc) {
    wchar_t localized_font_name[LF_FACESIZE];
    HFONT original_font = reinterpret_cast<HFONT>(SelectObject(memory_dc, hf));
    int length = GetTextFace(memory_dc, arraysize(localized_font_name),
                             &localized_font_name[0]);
    if (length > 0) {
      memcpy(&logfont.lfFaceName[0], &localized_font_name[0],
             sizeof(localized_font_name));
    }
    SelectObject(memory_dc, original_font);
    DeleteDC(memory_dc);
  }
  CHOOSEFONT cf;
  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = run_state.owner;
  cf.lpLogFont = &logfont;
  // Limit the list to a reasonable subset of fonts.
  // TODO : get rid of style selector and script selector
  // 1. List only truetype font
  // 2. Exclude vertical fonts (whose names begin with '@')
  // 3. Exclude symbol and OEM fonts
  // 4. Limit the size to [8, 40].
  // See http://msdn.microsoft.com/en-us/library/ms646832(VS.85).aspx
  cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_TTONLY |
      CF_NOVERTFONTS | CF_SCRIPTSONLY | CF_LIMITSIZE;

  // These limits are arbitrary and needs to be revisited. Is it bad
  // to clamp the size at 40 from A11Y point of view?
  cf.nSizeMin = 8;
  cf.nSizeMax = 40;

  bool success = !!ChooseFont(&cf);
  DisableOwner(run_state.owner);
  if (success) {
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &SelectFontDialogImpl::FontSelected, logfont, params,
          run_state));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &SelectFontDialogImpl::FontNotSelected, params,
        run_state));
  }
}

void SelectFontDialogImpl::FontSelected(LOGFONT logfont,
                                        void* params,
                                        RunState run_state) {
  if (listener_) {
    HFONT font = CreateFontIndirect(&logfont);
    if (font) {
      listener_->FontSelected(gfx::Font(font), params);
      DeleteObject(font);
    } else {
      listener_->FontSelectionCanceled(params);
    }
  }
  EndRun(run_state);
}

void SelectFontDialogImpl::FontNotSelected(void* params, RunState run_state) {
  if (listener_)
    listener_->FontSelectionCanceled(params);
  EndRun(run_state);
}

// static
SelectFontDialog* SelectFontDialog::Create(Listener* listener) {
  return new SelectFontDialogImpl(listener);
}
