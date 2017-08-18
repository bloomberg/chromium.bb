// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/value_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

using base::Bind;
using base::Callback;
using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::RenderViewHost;
using content::WebContents;
using std::set;

namespace {

static const char kRootName[] = "<root>";

base::LazyInstance<base::FilePath>::Leaky
    g_last_save_path = LAZY_INSTANCE_INITIALIZER;

typedef Callback<void(const base::FilePath&)> SelectedCallback;
typedef Callback<void(void)> CanceledCallback;

class SelectFileDialog : public ui::SelectFileDialog::Listener,
                         public base::RefCounted<SelectFileDialog> {
 public:
  SelectFileDialog(const SelectedCallback& selected_callback,
                   const CanceledCallback& canceled_callback,
                   WebContents* web_contents)
      : selected_callback_(selected_callback),
        canceled_callback_(canceled_callback),
        web_contents_(web_contents) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  }

  void Show(ui::SelectFileDialog::Type type,
            const base::FilePath& default_path) {
    AddRef();  // Balanced in the three listener outcomes.
    select_file_dialog_->SelectFile(
      type,
      base::string16(),
      default_path,
      NULL,
      0,
      base::FilePath::StringType(),
      platform_util::GetTopLevel(web_contents_->GetNativeView()),
      NULL);
  }

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    selected_callback_.Run(path);
    Release();  // Balanced in ::Show.
  }

  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    Release();  // Balanced in ::Show.
    NOTREACHED() << "Should not be able to select multiple files";
  }

  void FileSelectionCanceled(void* params) override {
    if (!canceled_callback_.is_null())
      canceled_callback_.Run();
    Release();  // Balanced in ::Show.
  }

 private:
  friend class base::RefCounted<SelectFileDialog>;
  ~SelectFileDialog() override {}

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  SelectedCallback selected_callback_;
  CanceledCallback canceled_callback_;
  WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialog);
};

void WriteToFile(const base::FilePath& path, const std::string& content) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!path.empty());

  base::WriteFile(path, content.c_str(), content.length());
}

void AppendToFile(const base::FilePath& path, const std::string& content) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!path.empty());

  base::AppendToFile(path, content.c_str(), content.size());
}

storage::IsolatedContext* isolated_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  return isolated_context;
}

std::string RegisterFileSystem(WebContents* web_contents,
                               const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  std::string root_name(kRootName);
  std::string file_system_id = isolated_context()->RegisterFileSystemForPath(
      storage::kFileSystemTypeNativeLocal, std::string(), path, &root_name);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  int renderer_id = render_view_host->GetProcess()->GetID();
  policy->GrantReadFileSystem(renderer_id, file_system_id);
  policy->GrantWriteFileSystem(renderer_id, file_system_id);
  policy->GrantCreateFileForFileSystem(renderer_id, file_system_id);
  policy->GrantDeleteFromFileSystem(renderer_id, file_system_id);

  // We only need file level access for reading FileEntries. Saving FileEntries
  // just needs the file system to have read/write access, which is granted
  // above if required.
  if (!policy->CanReadFile(renderer_id, path))
    policy->GrantReadFile(renderer_id, path);
  return file_system_id;
}

DevToolsFileHelper::FileSystem CreateFileSystemStruct(
    WebContents* web_contents,
    const std::string& file_system_id,
    const std::string& file_system_path) {
  const GURL origin = web_contents->GetURL().GetOrigin();
  std::string file_system_name =
      storage::GetIsolatedFileSystemName(origin, file_system_id);
  std::string root_url = storage::GetIsolatedFileSystemRootURIString(
      origin, file_system_id, kRootName);
  return DevToolsFileHelper::FileSystem(file_system_name,
                                        root_url,
                                        file_system_path);
}

set<std::string> GetAddedFileSystemPaths(Profile* profile) {
  const base::DictionaryValue* file_systems_paths_value =
      profile->GetPrefs()->GetDictionary(prefs::kDevToolsFileSystemPaths);
  set<std::string> result;
  for (base::DictionaryValue::Iterator it(*file_systems_paths_value);
       !it.IsAtEnd(); it.Advance()) {
    result.insert(it.key());
  }
  return result;
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
                                       Profile* profile,
                                       Delegate* delegate)
    : web_contents_(web_contents),
      profile_(profile),
      delegate_(delegate),
      file_task_runner_(
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})),
      weak_factory_(this) {
  pref_change_registrar_.Init(profile_->GetPrefs());
}

DevToolsFileHelper::~DevToolsFileHelper() = default;

void DevToolsFileHelper::Save(const std::string& url,
                              const std::string& content,
                              bool save_as,
                              const SaveCallback& saveCallback,
                              const SaveCallback& cancelCallback) {
  PathsMap::iterator it = saved_files_.find(url);
  if (it != saved_files_.end() && !save_as) {
    SaveAsFileSelected(url, content, saveCallback, it->second);
    return;
  }

  const base::DictionaryValue* file_map =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsEditedFiles);
  base::FilePath initial_path;

  const base::Value* path_value;
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
           saveCallback),
      cancelCallback,
      web_contents_);
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
  file_task_runner_->PostTask(FROM_HERE,
                              BindOnce(&AppendToFile, it->second, content));
}

void DevToolsFileHelper::SaveAsFileSelected(const std::string& url,
                                            const std::string& content,
                                            const SaveCallback& callback,
                                            const base::FilePath& path) {
  *g_last_save_path.Pointer() = path;
  saved_files_[url] = path;

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsEditedFiles);
  base::DictionaryValue* files_map = update.Get();
  files_map->SetWithoutPathExpansion(base::MD5String(url),
                                     base::CreateFilePathValue(path));
  callback.Run();
  file_task_runner_->PostTask(FROM_HERE, BindOnce(&WriteToFile, path, content));
}

void DevToolsFileHelper::AddFileSystem(
    const std::string& file_system_path,
    const ShowInfoBarCallback& show_info_bar_callback) {
  if (file_system_path.empty()) {
    scoped_refptr<SelectFileDialog> select_file_dialog = new SelectFileDialog(
        Bind(&DevToolsFileHelper::InnerAddFileSystem,
             weak_factory_.GetWeakPtr(), show_info_bar_callback),
        base::Closure(),
        web_contents_);
    select_file_dialog->Show(ui::SelectFileDialog::SELECT_FOLDER,
                             base::FilePath());
  } else {
    file_task_runner_->PostTask(
        FROM_HERE,
        BindOnce(&DevToolsFileHelper::CheckProjectFileExistsAndAddFileSystem,
                 weak_factory_.GetWeakPtr(), show_info_bar_callback,
                 base::FilePath::FromUTF8Unsafe(file_system_path)));
  }
}

// static
void DevToolsFileHelper::CheckProjectFileExistsAndAddFileSystem(
    base::WeakPtr<DevToolsFileHelper> self,
    ShowInfoBarCallback show_info_bar_callback,
    base::FilePath path) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (base::PathExists(path.Append(FILE_PATH_LITERAL(".devtools")))) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        BindOnce(&DevToolsFileHelper::InnerAddFileSystem, std::move(self),
                 std::move(show_info_bar_callback), path));
  }
}

void DevToolsFileHelper::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url,
    const ShowInfoBarCallback& show_info_bar_callback) {
  storage::FileSystemURL root_url =
      isolated_context()->CrackURL(GURL(file_system_url));
  if (!root_url.is_valid() || !root_url.path().empty())
    return;

  std::vector<storage::MountPoints::MountPointInfo> mount_points;
  isolated_context()->GetDraggedFileInfo(root_url.filesystem_id(),
                                         &mount_points);

  std::vector<storage::MountPoints::MountPointInfo>::const_iterator it =
      mount_points.begin();
  for (; it != mount_points.end(); ++it)
    InnerAddFileSystem(show_info_bar_callback, it->path);
}

void DevToolsFileHelper::InnerAddFileSystem(
    const ShowInfoBarCallback& show_info_bar_callback,
    const base::FilePath& path) {
  std::string file_system_path = path.AsUTF8Unsafe();

  const base::DictionaryValue* file_systems_paths_value =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsFileSystemPaths);
  if (file_systems_paths_value->HasKey(file_system_path))
    return;

  std::string path_display_name = path.AsEndingWithSeparator().AsUTF8Unsafe();
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE,
      base::UTF8ToUTF16(path_display_name));
  show_info_bar_callback.Run(
      message,
      Bind(&DevToolsFileHelper::AddUserConfirmedFileSystem,
           weak_factory_.GetWeakPtr(), path));
}

void DevToolsFileHelper::AddUserConfirmedFileSystem(
    const base::FilePath& path,
    bool allowed) {
  if (!allowed)
    return;

  std::string file_system_id = RegisterFileSystem(web_contents_, path);
  std::string file_system_path = path.AsUTF8Unsafe();

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  base::DictionaryValue* file_systems_paths_value = update.Get();
  file_systems_paths_value->SetWithoutPathExpansion(
      file_system_path, base::MakeUnique<base::Value>());
}

std::vector<DevToolsFileHelper::FileSystem>
DevToolsFileHelper::GetFileSystems() {
  file_system_paths_ = GetAddedFileSystemPaths(profile_);
  std::vector<FileSystem> file_systems;
  if (!file_watcher_) {
    file_watcher_.reset(new DevToolsFileWatcher(
        base::Bind(&DevToolsFileHelper::FilePathsChanged,
                   weak_factory_.GetWeakPtr()),
        base::SequencedTaskRunnerHandle::Get()));
    pref_change_registrar_.Add(
        prefs::kDevToolsFileSystemPaths,
        base::Bind(&DevToolsFileHelper::FileSystemPathsSettingChanged,
                   base::Unretained(this)));
  }
  for (auto file_system_path : file_system_paths_) {
    base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
    std::string file_system_id = RegisterFileSystem(web_contents_, path);
    FileSystem filesystem = CreateFileSystemStruct(web_contents_,
                                                   file_system_id,
                                                   file_system_path);
    file_systems.push_back(filesystem);
    file_watcher_->AddWatch(std::move(path));
  }
  return file_systems;
}

void DevToolsFileHelper::RemoveFileSystem(const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
  isolated_context()->RevokeFileSystemByPath(path);

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  base::DictionaryValue* file_systems_paths_value = update.Get();
  file_systems_paths_value->RemoveWithoutPathExpansion(file_system_path, NULL);
}

bool DevToolsFileHelper::IsFileSystemAdded(
    const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  set<std::string> file_system_paths = GetAddedFileSystemPaths(profile_);
  return file_system_paths.find(file_system_path) != file_system_paths.end();
}

void DevToolsFileHelper::FileSystemPathsSettingChanged() {
  std::set<std::string> remaining;
  remaining.swap(file_system_paths_);
  DCHECK(file_watcher_.get());

  for (auto file_system_path : GetAddedFileSystemPaths(profile_)) {
    if (remaining.find(file_system_path) == remaining.end()) {
      base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
      std::string file_system_id = RegisterFileSystem(web_contents_, path);
      FileSystem filesystem = CreateFileSystemStruct(web_contents_,
                                                     file_system_id,
                                                     file_system_path);
      delegate_->FileSystemAdded(filesystem);
      file_watcher_->AddWatch(std::move(path));
    } else {
      remaining.erase(file_system_path);
    }
    file_system_paths_.insert(file_system_path);
  }

  for (auto file_system_path : remaining) {
    delegate_->FileSystemRemoved(file_system_path);
    base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
    file_watcher_->RemoveWatch(std::move(path));
  }
}

void DevToolsFileHelper::FilePathsChanged(
    const std::vector<std::string>& changed_paths,
    const std::vector<std::string>& added_paths,
    const std::vector<std::string>& removed_paths) {
  delegate_->FilePathsChanged(changed_paths, added_paths, removed_paths);
}
