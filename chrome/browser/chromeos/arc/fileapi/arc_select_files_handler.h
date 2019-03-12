// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/arc/common/file_system.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class SelectFileDialogScriptExecutor;

// Exposed for testing.
extern const char kScriptClickOk[];
extern const char kScriptClickDirectory[];
extern const char kScriptClickFile[];
extern const char kScriptGetElements[];

// Handler for FileSystemHost.SelectFiles.
class ArcSelectFilesHandler : public ui::SelectFileDialog::Listener {
 public:
  explicit ArcSelectFilesHandler(content::BrowserContext* context);
  ~ArcSelectFilesHandler() override;

  void SelectFiles(const mojom::SelectFilesRequestPtr& request,
                   mojom::FileSystemHost::SelectFilesCallback callback);

  void OnFileSelectorEvent(
      mojom::FileSelectorEventPtr event,
      mojom::FileSystemHost::OnFileSelectorEventCallback callback);

  void GetFileSelectorElements(
      mojom::FileSystemHost::GetFileSelectorElementsCallback callback);

  // ui::SelectFileDialog::Listener overrides:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

  void SetSelectFileDialogForTesting(ui::SelectFileDialog* dialog);
  void SetDialogScriptExecutorForTesting(
      SelectFileDialogScriptExecutor* dialog_script_executor);

 private:
  friend class ArcSelectFilesHandlerTest;

  void FilesSelectedInternal(const std::vector<base::FilePath>& files,
                             void* params);

  Profile* const profile_;

  mojom::FileSystemHost::SelectFilesCallback callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  scoped_refptr<SelectFileDialogScriptExecutor> dialog_script_executor_;

  DISALLOW_COPY_AND_ASSIGN(ArcSelectFilesHandler);
};

// Helper class for executing JavaScript on a given SelectFileDialog.
class SelectFileDialogScriptExecutor
    : public base::RefCounted<SelectFileDialogScriptExecutor> {
 public:
  explicit SelectFileDialogScriptExecutor(
      ui::SelectFileDialog* select_file_dialog);

  virtual void ExecuteJavaScript(
      const std::string& script,
      content::RenderFrameHost::JavaScriptResultCallback callback);

 protected:
  friend class base::RefCounted<SelectFileDialogScriptExecutor>;

  virtual ~SelectFileDialogScriptExecutor();

 private:
  ui::SelectFileDialog* select_file_dialog_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
