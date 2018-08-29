// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/file_system_chooser.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

// static
void FileSystemChooser::CreateAndShow(
    int render_process_id,
    int frame_id,
    ResultCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(render_process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  auto* listener = new FileSystemChooser(render_process_id, std::move(callback),
                                         std::move(callback_runner));
  listener->dialog_ = ui::SelectFileDialog::Create(
      listener,
      GetContentClient()->browser()->CreateSelectFilePolicy(web_contents));
  // TODO(https://crbug.com/878581): Better/more specific options to pass to
  //     SelectFile.
  listener->dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, /*title=*/base::string16(),
      /*default_path=*/base::FilePath(), /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(),
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr,
      /*params=*/nullptr);
}

FileSystemChooser::FileSystemChooser(
    int render_process_id,
    ResultCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner)
    : render_process_id_(render_process_id),
      callback_(std::move(callback)),
      callback_runner_(std::move(callback_runner)) {}

FileSystemChooser::~FileSystemChooser() {
  if (dialog_)
    dialog_->ListenerDestroyed();
}

void FileSystemChooser::FileSelected(const base::FilePath& path,
                                     int index,
                                     void* params) {
  MultiFilesSelected({path}, params);
}

void FileSystemChooser::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  auto* isolated_context = storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  auto* security_policy = ChildProcessSecurityPolicy::GetInstance();
  DCHECK(security_policy);

  std::vector<blink::mojom::FileSystemEntryPtr> result;
  result.reserve(files.size());
  for (const auto& path : files) {
    auto entry = blink::mojom::FileSystemEntry::New();
    entry->file_system_id = isolated_context->RegisterFileSystemForPath(
        storage::kFileSystemTypeNativeForPlatformApp, std::string(), path,
        &entry->base_name);

    // TODO(https://crbug.com/878585): Determine if we always want to grant
    //     write permission.
    security_policy->GrantReadFileSystem(render_process_id_,
                                         entry->file_system_id);
    security_policy->GrantWriteFileSystem(render_process_id_,
                                          entry->file_system_id);
    security_policy->GrantDeleteFromFileSystem(render_process_id_,
                                               entry->file_system_id);

    result.push_back(std::move(entry));
  }

  callback_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), base::File::FILE_OK,
                                std::move(result)));
  delete this;
}

void FileSystemChooser::FileSelectionCanceled(void* params) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), base::File::FILE_ERROR_ABORT,
                     std::vector<blink::mojom::FileSystemEntryPtr>()));
  delete this;
}

}  // namespace content
