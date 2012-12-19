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
#include "base/value_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/dialogs/select_file_dialog.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;

namespace {

base::LazyInstance<FilePath>::Leaky
    g_last_save_path = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class DevToolsFileHelper::SaveAsDialog : public ui::SelectFileDialog::Listener,
                                         public base::RefCounted<SaveAsDialog> {
 public:
  explicit SaveAsDialog(DevToolsFileHelper* helper)
      : helper_(helper) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(NULL));
  }

  void ResetHelper() {
    helper_ = NULL;
  }

  void Show(const std::string& url,
            const FilePath& initial_path,
            const std::string& content) {
    AddRef();  // Balanced in the three listener outcomes.

    url_ = url;
    content_ = content;

    select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    initial_path,
                                    NULL,
                                    0,
                                    FILE_PATH_LITERAL(""),
                                    NULL,
                                    NULL);
  }

  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) {
    if (helper_)
      helper_->FileSelected(url_, path, content_);
    Release();  // Balanced in ::Show.
  }

  virtual void MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
    Release();  // Balanced in ::Show.
    NOTREACHED() << "Should not be able to select multiple files";
  }

  virtual void FileSelectionCanceled(void* params) {
    Release();  // Balanced in ::Show.
  }

 private:
  friend class base::RefCounted<SaveAsDialog>;
  virtual ~SaveAsDialog() {}

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::string url_;
  std::string content_;
  DevToolsFileHelper* helper_;
};

// static
void DevToolsFileHelper::WriteFile(const FilePath& path,
                                   const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!path.empty());

  file_util::WriteFile(path, content.c_str(), content.length());
}

// static
void DevToolsFileHelper::AppendToFile(const FilePath& path,
                                    const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!path.empty());

  file_util::AppendToFile(path, content.c_str(), content.length());
}

DevToolsFileHelper::DevToolsFileHelper(Profile* profile, Delegate* delegate)
    : profile_(profile),
      delegate_(delegate) {
}

DevToolsFileHelper::~DevToolsFileHelper() {
  if (save_as_dialog_)
    save_as_dialog_->ResetHelper();
}

void DevToolsFileHelper::Save(const std::string& url,
                              const std::string& content,
                              bool save_as) {
  PathsMap::iterator it = saved_files_.find(url);
  if (it != saved_files_.end() && !save_as) {
    FileSelected(url, it->second, content);
    return;
  }

  if (save_as_dialog_)
    save_as_dialog_->ResetHelper();

  const DictionaryValue* file_map =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsEditedFiles);
  FilePath initial_path;

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
      FilePath download_path = DownloadPrefs::FromDownloadManager(
          BrowserContext::GetDownloadManager(profile_))->DownloadPath();
      initial_path = download_path.AppendASCII(suggested_file_name);
    }
  }

  save_as_dialog_ = new SaveAsDialog(this);
  save_as_dialog_->Show(url, initial_path, content);
}

void DevToolsFileHelper::Append(const std::string& url,
                                const std::string& content) {
  PathsMap::iterator it = saved_files_.find(url);
  if (it == saved_files_.end())
    return;

  delegate_->AppendedTo(url);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsFileHelper::AppendToFile,
                 it->second,
                 content));
}

void DevToolsFileHelper::FileSelected(const std::string& url,
                                      const FilePath& path,
                                      const std::string& content) {
  *g_last_save_path.Pointer() = path;
  saved_files_[url] = path;

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsEditedFiles);
  DictionaryValue* files_map = update.Get();
  files_map->SetWithoutPathExpansion(base::MD5String(url),
                                     base::CreateFilePathValue(path));
  delegate_->FileSavedAs(url);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsFileHelper::WriteFile,
                 path,
                 content));
}
