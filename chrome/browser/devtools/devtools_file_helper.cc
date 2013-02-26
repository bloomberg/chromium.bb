// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/md5.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

using base::Bind;
using base::Callback;
using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::RenderViewHost;
using content::WebContents;

namespace {

base::LazyInstance<base::FilePath>::Leaky
    g_last_save_path = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace {

typedef Callback<void(const base::FilePath&)> SelectedCallback;
typedef Callback<void(void)> CanceledCallback;

const base::FilePath::CharType kMagicFileName[] =
    FILE_PATH_LITERAL(".allow-devtools-edit");

class SelectFileDialog : public ui::SelectFileDialog::Listener,
                         public base::RefCounted<SelectFileDialog> {
 public:
  SelectFileDialog(const SelectedCallback& selected_callback,
                   const CanceledCallback& canceled_callback)
      : selected_callback_(selected_callback),
        canceled_callback_(canceled_callback) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(NULL));
  }

  void Show(ui::SelectFileDialog::Type type,
            const base::FilePath& default_path) {
    AddRef();  // Balanced in the three listener outcomes.
    select_file_dialog_->SelectFile(type,
                                    string16(),
                                    default_path,
                                    NULL,
                                    0,
                                    FILE_PATH_LITERAL(""),
                                    NULL,
                                    NULL);
  }

  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    selected_callback_.Run(path);
    Release();  // Balanced in ::Show.
  }

  virtual void MultiFilesSelected(const std::vector<base::FilePath>& files,
                                  void* params) OVERRIDE {
    Release();  // Balanced in ::Show.
    NOTREACHED() << "Should not be able to select multiple files";
  }

  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    canceled_callback_.Run();
    Release();  // Balanced in ::Show.
  }

 private:
  friend class base::RefCounted<SelectFileDialog>;
  virtual ~SelectFileDialog() {}

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  SelectedCallback selected_callback_;
  CanceledCallback canceled_callback_;
};

void WriteToFile(const base::FilePath& path, const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!path.empty());

  file_util::WriteFile(path, content.c_str(), content.length());
}

void AppendToFile(const base::FilePath& path, const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!path.empty());

  file_util::AppendToFile(path, content.c_str(), content.length());
}

fileapi::IsolatedContext* isolated_context() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  return isolated_context;
}

std::string RegisterFileSystem(WebContents* web_contents,
                               const base::FilePath& path,
                               std::string* registered_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(content::HasWebUIScheme(web_contents->GetURL()));
  std::string file_system_id = isolated_context()->RegisterFileSystemForPath(
      fileapi::kFileSystemTypeNativeLocal, path, registered_name);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  int renderer_id = render_view_host->GetProcess()->GetID();
  policy->GrantReadFileSystem(renderer_id, file_system_id);
  policy->GrantWriteFileSystem(renderer_id, file_system_id);

  // We only need file level access for reading FileEntries. Saving FileEntries
  // just needs the file system to have read/write access, which is granted
  // above if required.
  if (!policy->CanReadFile(renderer_id, path))
    policy->GrantReadFile(renderer_id, path);

  return file_system_id;
}

typedef Callback<void(const std::vector<base::FilePath>&)>
    ValidateFoldersCallback;

void ValidateFoldersOnFileThread(const std::vector<base::FilePath>& file_paths,
                                 const ValidateFoldersCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<base::FilePath> permitted_paths;
  std::vector<base::FilePath>::const_iterator it;
  for (it = file_paths.begin(); it != file_paths.end(); ++it) {
    base::FilePath security_file_path = it->Append(kMagicFileName);
    if (file_util::PathExists(security_file_path))
      permitted_paths.push_back(*it);
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          Bind(callback, permitted_paths));
}

DevToolsFileHelper::FileSystem CreateFileSystemStruct(
    WebContents* web_contents,
    const std::string& file_system_id,
    const std::string& registered_name,
    const std::string& file_system_path) {
  const GURL origin = web_contents->GetURL().GetOrigin();
  std::string file_system_name = fileapi::GetIsolatedFileSystemName(
      origin,
      file_system_id);
  std::string root_url = fileapi::GetIsolatedFileSystemRootURIString(
      origin,
      file_system_id,
      registered_name);
  return DevToolsFileHelper::FileSystem(file_system_name,
                                        root_url,
                                        file_system_path);
}

}  // namespace

DevToolsFileHelper::FileSystem::FileSystem() {
}

DevToolsFileHelper::FileSystem::FileSystem(const std::string& file_system_name,
                                           const std::string& root_url,
                                           const std::string& file_system_path)
    : file_system_name(file_system_name),
      root_url(root_url),
      file_system_path(file_system_path) {
}

DevToolsFileHelper::DevToolsFileHelper(WebContents* web_contents,
                                       Profile* profile)
    : web_contents_(web_contents),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

DevToolsFileHelper::~DevToolsFileHelper() {
}

void DevToolsFileHelper::Save(const std::string& url,
                              const std::string& content,
                              bool save_as,
                              const SaveCallback& callback) {
  PathsMap::iterator it = saved_files_.find(url);
  if (it != saved_files_.end() && !save_as) {
    SaveAsFileSelected(url, content, callback, it->second);
    return;
  }

  const DictionaryValue* file_map =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsEditedFiles);
  base::FilePath initial_path;

  const Value* path_value;
  if (file_map->Get(base::MD5String(url), &path_value))
    base::GetValueAsFilePath(*path_value, &initial_path);

  if (initial_path.empty()) {
    GURL gurl(url);
    std::string suggested_file_name = gurl.is_valid() ?
        gurl.ExtractFileName() : url;

    if (suggested_file_name.length() > 64)
      suggested_file_name = suggested_file_name.substr(0, 64);

    if (!g_last_save_path.Pointer()->empty()) {
      initial_path = g_last_save_path.Pointer()->DirName().AppendASCII(
          suggested_file_name);
    } else {
      base::FilePath download_path = DownloadPrefs::FromDownloadManager(
          BrowserContext::GetDownloadManager(profile_))->DownloadPath();
      initial_path = download_path.AppendASCII(suggested_file_name);
    }
  }

  scoped_refptr<SelectFileDialog> select_file_dialog = new SelectFileDialog(
      Bind(&DevToolsFileHelper::SaveAsFileSelected,
           weak_factory_.GetWeakPtr(),
           url,
           content,
           callback),
      Bind(&DevToolsFileHelper::SaveAsFileSelectionCanceled,
           weak_factory_.GetWeakPtr()));
  select_file_dialog->Show(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                           initial_path);
}

void DevToolsFileHelper::Append(const std::string& url,
                                const std::string& content,
                                const AppendCallback& callback) {
  PathsMap::iterator it = saved_files_.find(url);
  if (it == saved_files_.end())
    return;
  callback.Run();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          Bind(&AppendToFile, it->second, content));
}

void DevToolsFileHelper::SaveAsFileSelected(const std::string& url,
                                            const std::string& content,
                                            const SaveCallback& callback,
                                            const base::FilePath& path) {
  *g_last_save_path.Pointer() = path;
  saved_files_[url] = path;

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsEditedFiles);
  DictionaryValue* files_map = update.Get();
  files_map->SetWithoutPathExpansion(base::MD5String(url),
                                     base::CreateFilePathValue(path));
  callback.Run();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          Bind(&WriteToFile, path, content));
}

void DevToolsFileHelper::SaveAsFileSelectionCanceled() {
}

void DevToolsFileHelper::AddFileSystem(const AddFileSystemCallback& callback) {
  scoped_refptr<SelectFileDialog> select_file_dialog = new SelectFileDialog(
      Bind(&DevToolsFileHelper::InnerAddFileSystem,
           weak_factory_.GetWeakPtr(),
           callback),
      Bind(callback, "", FileSystem()));
  select_file_dialog->Show(ui::SelectFileDialog::SELECT_FOLDER,
                           base::FilePath());
}

void DevToolsFileHelper::InnerAddFileSystem(
    const AddFileSystemCallback& callback,
    const base::FilePath& path) {
  std::vector<base::FilePath> file_paths(1, path);
  ValidateFoldersCallback validate_folders_callback = Bind(
      &DevToolsFileHelper::AddValidatedFileSystem,
      weak_factory_.GetWeakPtr(),
      callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          Bind(&ValidateFoldersOnFileThread,
                               file_paths,
                               validate_folders_callback));
}

void DevToolsFileHelper::AddValidatedFileSystem(
    const AddFileSystemCallback& callback,
    const std::vector<base::FilePath>& permitted_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (permitted_paths.empty()) {
    std::string magic_file_name = base::FilePath(kMagicFileName).AsUTF8Unsafe();
    std::string error_string = l10n_util::GetStringFUTF8(
        IDS_DEV_TOOLS_MAGIC_FILE_NOT_EXISTS_MESSAGE,
        UTF8ToUTF16(magic_file_name));
    callback.Run(error_string, FileSystem());
    return;
  }
  base::FilePath path = permitted_paths.at(0);
  std::string registered_name;
  std::string file_system_id = RegisterFileSystem(web_contents_,
                                                  path,
                                                  &registered_name);
  std::string file_system_path = path.AsUTF8Unsafe();

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  DictionaryValue* file_systems_paths_value = update.Get();
  file_systems_paths_value->Set(file_system_path, Value::CreateNullValue());

  FileSystem filesystem = CreateFileSystemStruct(web_contents_,
                                                 file_system_id,
                                                 registered_name,
                                                 file_system_path);
  callback.Run("", filesystem);
}

void DevToolsFileHelper::RequestFileSystems(
    const RequestFileSystemsCallback& callback) {
  const DictionaryValue* file_systems_paths_value =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsFileSystemPaths);
  std::vector<base::FilePath> saved_paths;
  for (DictionaryValue::Iterator it(*file_systems_paths_value); !it.IsAtEnd();
       it.Advance()) {
    std::string file_system_path = it.key();
    base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
    saved_paths.push_back(path);
  }

  ValidateFoldersCallback validate_folders_callback = Bind(
      &DevToolsFileHelper::RestoreValidatedFileSystems,
      weak_factory_.GetWeakPtr(),
      callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          Bind(&ValidateFoldersOnFileThread,
                               saved_paths,
                               validate_folders_callback));
}

void DevToolsFileHelper::RestoreValidatedFileSystems(
    const RequestFileSystemsCallback& callback,
    const std::vector<base::FilePath>& permitted_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<FileSystem> file_systems;
  std::vector<base::FilePath>::const_iterator it;
  for (it = permitted_paths.begin(); it != permitted_paths.end(); ++it) {
    std::string registered_name;
    std::string file_system_id = RegisterFileSystem(web_contents_,
                                                    *it,
                                                    &registered_name);
    std::string file_system_path = it->AsUTF8Unsafe();
    FileSystem filesystem = CreateFileSystemStruct(web_contents_,
                                                   file_system_id,
                                                   registered_name,
                                                   file_system_path);
    file_systems.push_back(filesystem);
  }
  callback.Run(file_systems);
}

void DevToolsFileHelper::RemoveFileSystem(const std::string& file_system_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
  isolated_context()->RevokeFileSystemByPath(path);

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  DictionaryValue* file_systems_paths_value = update.Get();
  file_systems_paths_value->RemoveWithoutPathExpansion(file_system_path, NULL);
}
