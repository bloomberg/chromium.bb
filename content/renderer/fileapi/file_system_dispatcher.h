// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FILEAPI_FILE_SYSTEM_DISPATCHER_H_
#define CONTENT_RENDERER_FILEAPI_FILE_SYSTEM_DISPATCHER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/platform/web_file_system.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

// Dispatches and sends file system related messages sent to/from a child
// process from/to the main browser process. There is an instance held by
// each WebFileSystemImpl object.
// TODO(adithyas): Move functionality to blink::FileSystemDispatcher and
// remove this class.
class FileSystemDispatcher {
 public:
  explicit FileSystemDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~FileSystemDispatcher();

  using ChooseEntryCallbacks = blink::WebFileSystem::ChooseEntryCallbacks;
  void ChooseEntry(int render_frame_id,
                   std::unique_ptr<ChooseEntryCallbacks> callbacks);

 private:
  blink::mojom::FileSystemManager& GetFileSystemManager();

  blink::mojom::FileSystemManagerPtr file_system_manager_ptr_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FILEAPI_FILE_SYSTEM_DISPATCHER_H_
