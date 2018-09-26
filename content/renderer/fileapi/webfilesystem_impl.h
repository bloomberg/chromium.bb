// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FILEAPI_WEBFILESYSTEM_IMPL_H_
#define CONTENT_RENDERER_FILEAPI_WEBFILESYSTEM_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/fileapi/file_system_dispatcher.h"
#include "third_party/blink/public/platform/web_file_system.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// TODO(adithyas): Move functionality to blink::FileSystemDispatcher and remove
// this class.
class WebFileSystemImpl : public blink::WebFileSystem,
                          public WorkerThread::Observer {
 public:
  // Returns thread-specific instance.  If non-null |main_thread_loop|
  // is given and no thread-specific instance has been created it may
  // create a new instance.
  static WebFileSystemImpl* ThreadSpecificInstance(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  // Deletes thread-specific instance (if exists). For workers it deletes
  // itself in WillStopCurrentWorkerThread(), but for an instance created on the
  // main thread this method must be called.
  static void DeleteThreadSpecificInstance();

  explicit WebFileSystemImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~WebFileSystemImpl() override;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void ChooseEntry(blink::WebFrame* frame,
                   std::unique_ptr<ChooseEntryCallbacks>) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  FileSystemDispatcher file_system_dispatcher_;

  // Thread-affine per use of TLS in impl.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(WebFileSystemImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FILEAPI_WEBFILESYSTEM_IMPL_H_
