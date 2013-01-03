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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/dialogs/select_file_dialog.h"

using base::Bind;
using base::Callback;
using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;

namespace {

base::LazyInstance<FilePath>::Leaky
    g_last_save_path = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace {

typedef Callback<void(const FilePath&)> SelectedCallback;
typedef Callback<void(void)> CanceledCallback;

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
            const FilePath& default_path) {
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
  virtual void FileSelected(const FilePath& path, int index, void* params) {
    selected_callback_.Run(path);
    Release();  // Balanced in ::Show.
  }

  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) {
    Release();  // Balanced in ::Show.
    NOTREACHED() << "Should not be able to select multiple files";
  }

  virtual void FileSelectionCanceled(void* params) {
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

void WriteToFile(const FilePath& path, const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!path.empty());

  file_util::WriteFile(path, content.c_str(), content.length());
}

void AppendToFile(const FilePath& path, const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!path.empty());

  file_util::AppendToFile(path, content.c_str(), content.length());
}

}  // namespace

DevToolsFileHelper::DevToolsFileHelper(Profile* profile) : profile_(profile),
                                                           weak_factory_(this) {
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
                          base::Bind(&AppendToFile, it->second, content));
}

void DevToolsFileHelper::SaveAsFileSelected(const std::string& url,
                                            const std::string& content,
                                            const SaveCallback& callback,
                                            const FilePath& path) {
  *g_last_save_path.Pointer() = path;
  saved_files_[url] = path;

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsEditedFiles);
  DictionaryValue* files_map = update.Get();
  files_map->SetWithoutPathExpansion(base::MD5String(url),
                                     base::CreateFilePathValue(path));
  callback.Run();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&WriteToFile, path, content));
}

void DevToolsFileHelper::SaveAsFileSelectionCanceled() {
}
