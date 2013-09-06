// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_
#define CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"
#include "webkit/child/worker_task_runner.h"

namespace base {
class MessageLoopProxy;
}

namespace WebKit {
class WebURL;
class WebFileWriter;
class WebFileWriterClient;
}

// TODO(kinuko): Remove this hack after the two-sided patch lands.
#ifdef NON_SELFDESTRUCT_WEBFILESYSTEMCALLBACKS
  typedef WebKit::WebFileSystemCallbacks WebFileSystemCallbacksType;
#else
  typedef WebKit::WebFileSystemCallbacks* WebFileSystemCallbacksType;
#endif

namespace content {

class WebFileSystemImpl
    : public WebKit::WebFileSystem,
      public webkit_glue::WorkerTaskRunner::Observer,
      public base::NonThreadSafe {
 public:
  // Returns thread-specific instance.  If non-null |main_thread_loop|
  // is given and no thread-specific instance has been created it may
  // create a new instance.
  static WebFileSystemImpl* ThreadSpecificInstance(
      base::MessageLoopProxy* main_thread_loop);

  // Deletes thread-specific instance (if exists). For workers it deletes
  // itself in OnWorkerRunLoopStopped(), but for an instance created on the
  // main thread this method must be called.
  static void DeleteThreadSpecificInstance();

  explicit WebFileSystemImpl(base::MessageLoopProxy* main_thread_loop);
  virtual ~WebFileSystemImpl();

  // webkit_glue::WorkerTaskRunner::Observer implementation.
  virtual void OnWorkerRunLoopStopped() OVERRIDE;

  // WebFileSystem implementation.
  virtual void openFileSystem(
      const WebKit::WebURL& storage_partition,
      const WebKit::WebFileSystemType type,
      bool create,
      WebFileSystemCallbacksType);
  virtual void deleteFileSystem(
      const WebKit::WebURL& storage_partition,
      const WebKit::WebFileSystemType type,
      WebFileSystemCallbacksType);
  virtual void move(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void copy(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void remove(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void removeRecursively(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void readMetadata(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void createFile(
      const WebKit::WebURL& path,
      bool exclusive,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void createDirectory(
      const WebKit::WebURL& path,
      bool exclusive,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void fileExists(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void directoryExists(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void readDirectory(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual WebKit::WebFileWriter* createFileWriter(
      const WebKit::WebURL& path,
      WebKit::WebFileWriterClient*);
  virtual void createFileWriter(
      const WebKit::WebURL& path,
      WebKit::WebFileWriterClient*,
      WebFileSystemCallbacksType) OVERRIDE;
  virtual void createSnapshotFileAndReadMetadata(
      const WebKit::WebURL& path,
      WebFileSystemCallbacksType);

  int RegisterCallbacks(WebFileSystemCallbacksType callbacks);
  WebFileSystemCallbacksType GetAndUnregisterCallbacks(
      int callbacks_id);

 private:
  typedef std::map<int, WebFileSystemCallbacksType> CallbacksMap;

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_;

  CallbacksMap callbacks_;
  int next_callbacks_id_;

  DISALLOW_COPY_AND_ASSIGN(WebFileSystemImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_
