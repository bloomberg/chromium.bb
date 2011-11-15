// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_file_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/shell_dialogs.h"

using content::BrowserThread;

namespace {

base::LazyInstance<FilePath,
                   base::LeakyLazyInstanceTraits<FilePath> >
    g_last_save_path = LAZY_INSTANCE_INITIALIZER;

class SaveAsDialog : public SelectFileDialog::Listener,
                     public base::RefCounted<SaveAsDialog> {
 public:
  SaveAsDialog() {
    select_file_dialog_ = SelectFileDialog::Create(this);
  }

  void Show(Profile* profile,
            const std::string& suggested_file_name,
            const std::string& content) {
    AddRef();  // Balanced in the three listener outcomes.

    content_ = content;

    FilePath default_path;

    std::string file_name;
    if (suggested_file_name.length() > 20)
      file_name = suggested_file_name.substr(0, 20);
    else
      file_name = suggested_file_name;

    if (!g_last_save_path.Pointer()->empty()) {
      default_path = g_last_save_path.Pointer()->DirName().AppendASCII(
          suggested_file_name);
    } else {
      DownloadPrefs prefs(profile->GetPrefs());
      default_path = prefs.download_path().AppendASCII(suggested_file_name);
    }

    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    default_path,
                                    NULL,
                                    0,
                                    FILE_PATH_LITERAL(""),
                                    NULL,
                                    NULL,
                                    NULL);
  }

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) {
    *g_last_save_path.Pointer() = path;

    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveAsDialog::WriteFile, path, content_));
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

  static void WriteFile(FilePath path, const std::string& content) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(!path.empty());

    file_util::WriteFile(path, content.c_str(), content.length());
  }
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  std::string content_;
};

}  // namespace

// static
void DevToolsFileUtil::SaveAs(Profile* profile,
                              const std::string& suggested_file_name,
                              const std::string& content) {
  scoped_refptr<SaveAsDialog> dialog = new SaveAsDialog();
  dialog->Show(profile, suggested_file_name, content);
}
