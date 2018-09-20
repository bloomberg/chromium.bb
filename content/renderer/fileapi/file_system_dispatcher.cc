// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fileapi/file_system_dispatcher.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/worker_thread.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_type_converters.h"

namespace content {

FileSystemDispatcher::FileSystemDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(std::move(main_thread_task_runner)) {}

FileSystemDispatcher::~FileSystemDispatcher() = default;

blink::mojom::FileSystemManager& FileSystemDispatcher::GetFileSystemManager() {
  auto BindInterfaceOnMainThread =
      [](blink::mojom::FileSystemManagerRequest request) {
        DCHECK(ChildThreadImpl::current());
        ChildThreadImpl::current()->GetConnector()->BindInterface(
            mojom::kBrowserServiceName, std::move(request));
      };
  if (!file_system_manager_ptr_) {
    if (WorkerThread::GetCurrentId()) {
      blink::mojom::FileSystemManagerRequest request =
          mojo::MakeRequest(&file_system_manager_ptr_);
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(BindInterfaceOnMainThread, std::move(request)));
    } else {
      BindInterfaceOnMainThread(mojo::MakeRequest(&file_system_manager_ptr_));
    }
  }
  return *file_system_manager_ptr_;
}

void FileSystemDispatcher::ChooseEntry(
    int render_frame_id,
    std::unique_ptr<ChooseEntryCallbacks> callbacks) {
  GetFileSystemManager().ChooseEntry(
      render_frame_id,
      base::BindOnce(
          [](std::unique_ptr<ChooseEntryCallbacks> callbacks,
             base::File::Error result,
             std::vector<blink::mojom::FileSystemEntryPtr> entries) {
            if (result != base::File::FILE_OK) {
              callbacks->OnError(result);
            } else {
              blink::WebVector<blink::WebFileSystem::FileSystemEntry>
                  web_entries(entries.size());
              for (size_t i = 0; i < entries.size(); ++i) {
                web_entries[i].file_system_id =
                    blink::WebString::FromASCII(entries[i]->file_system_id);
                web_entries[i].base_name =
                    blink::WebString::FromASCII(entries[i]->base_name);
              }
              callbacks->OnSuccess(std::move(web_entries));
            }
          },
          std::move(callbacks)));
}

}  // namespace content
