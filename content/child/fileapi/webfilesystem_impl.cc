// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilesystem_impl.h"

#include "base/bind.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilesystem_callback_adapters.h"
#include "content/child/fileapi/webfilewriter_impl.h"
#include "content/common/fileapi/file_system_messages.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebFileSystemCallbacks.h"
#include "url/gurl.h"
#include "webkit/child/worker_task_runner.h"
#include "webkit/common/fileapi/directory_entry.h"
#include "webkit/common/fileapi/file_system_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;
using webkit_glue::WorkerTaskRunner;

namespace content {

namespace {

class CallbacksMap;

base::LazyInstance<base::ThreadLocalPointer<CallbacksMap> >::Leaky
    g_callbacks_map_tls = LAZY_INSTANCE_INITIALIZER;

// TODO(kinuko): Integrate this into WebFileSystemImpl when blink side
// becomes ready to make WebFileSystemImpl thread-local.
class CallbacksMap : public WorkerTaskRunner::Observer {
 public:
  static CallbacksMap* Get() {
    return g_callbacks_map_tls.Pointer()->Get();
  }

  static CallbacksMap* GetOrCreate() {
    if (g_callbacks_map_tls.Pointer()->Get())
      return g_callbacks_map_tls.Pointer()->Get();
    CallbacksMap* map = new CallbacksMap;
    if (WorkerTaskRunner::Instance()->CurrentWorkerId())
      WorkerTaskRunner::Instance()->AddStopObserver(map);
    return map;
  }

  virtual ~CallbacksMap() {
    IDMap<WebFileSystemCallbacks>::iterator iter(&callbacks_);
    while (!iter.IsAtEnd()) {
      iter.GetCurrentValue()->didFail(WebKit::WebFileErrorAbort);
      iter.Advance();
    }
    g_callbacks_map_tls.Pointer()->Set(NULL);
  }

  // webkit_glue::WorkerTaskRunner::Observer implementation.
  virtual void OnWorkerRunLoopStopped() OVERRIDE {
    delete this;
  }

  int RegisterCallbacks(WebFileSystemCallbacks* callbacks) {
    return callbacks_.Add(callbacks);
  }

  WebFileSystemCallbacks* GetAndUnregisterCallbacks(
      int callbacks_id) {
    WebFileSystemCallbacks* callbacks = callbacks_.Lookup(callbacks_id);
    callbacks_.Remove(callbacks_id);
    return callbacks;
  }

 private:
  CallbacksMap() {
    g_callbacks_map_tls.Pointer()->Set(this);
  }

  IDMap<WebFileSystemCallbacks> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CallbacksMap);
};

void DidReceiveSnapshotFile(int request_id) {
  if (ChildThread::current())
    ChildThread::current()->Send(
        new FileSystemHostMsg_DidReceiveSnapshotFile(request_id));
}

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

template <typename Method, typename Params>
void CallDispatcherOnMainThread(
    base::MessageLoopProxy* loop,
    Method method, const Params& params) {
  if (!loop->RunsTasksOnCurrentThread()) {
    loop->PostTask(FROM_HERE,
                   base::Bind(&CallDispatcherOnMainThread<Method, Params>,
                              make_scoped_refptr(loop), method, params));
    return;
  }
  if (!ChildThread::current() ||
      !ChildThread::current()->file_system_dispatcher())
    return;

  DispatchToMethod(ChildThread::current()->file_system_dispatcher(),
                   method, params);
}

template <typename Method, typename Params>
void CallbackFileSystemCallbacks(
    int thread_id, int callbacks_id,
    Method method, const Params& params) {
  if (thread_id != CurrentWorkerId()) {
    WorkerTaskRunner::Instance()->PostTask(
        thread_id,
        base::Bind(&CallbackFileSystemCallbacks<Method, Params>,
                   thread_id, callbacks_id, method, params));
    return;
  }
  if (!CallbacksMap::Get())
    return;

  WebFileSystemCallbacks* callbacks =
      CallbacksMap::Get()->GetAndUnregisterCallbacks(callbacks_id);
  DCHECK(callbacks);
  DispatchToMethod(callbacks, method, params);
}

void StatusCallbackAdapter(int thread_id, int callbacks_id,
                           base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK) {
    CallbackFileSystemCallbacks(
        thread_id, callbacks_id,
        &WebFileSystemCallbacks::didSucceed, MakeTuple());
  } else {
    CallbackFileSystemCallbacks(
        thread_id, callbacks_id,
        &WebFileSystemCallbacks::didFail,
        MakeTuple(fileapi::PlatformFileErrorToWebFileError(error)));
  }
}

void ReadMetadataCallbackAdapter(int thread_id, int callbacks_id,
                                 const base::PlatformFileInfo& file_info) {
  WebFileInfo web_file_info;
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id,
      &WebFileSystemCallbacks::didReadMetadata,
      MakeTuple(web_file_info));
}

void ReadDirectoryCallbackAdapater(
    int thread_id, int callbacks_id,
    const std::vector<fileapi::DirectoryEntry>& entries,
    bool has_more) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    file_system_entries[i].name =
        base::FilePath(entries[i].name).AsUTF16Unsafe();
    file_system_entries[i].isDirectory = entries[i].is_directory;
  }
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id,
      &WebFileSystemCallbacks::didReadDirectory,
      MakeTuple(file_system_entries, has_more));
}

void CreateFileWriterCallbackAdapter(
    int thread_id, int callbacks_id,
    base::MessageLoopProxy* main_thread_loop,
    const GURL& path,
    WebKit::WebFileWriterClient* client,
    const base::PlatformFileInfo& file_info) {
  if (thread_id != CurrentWorkerId()) {
    WorkerTaskRunner::Instance()->PostTask(
        thread_id,
        base::Bind(&CreateFileWriterCallbackAdapter,
                    thread_id, callbacks_id,
                    make_scoped_refptr(main_thread_loop),
                    path, client, file_info));
    return;
  }

  if (!CallbacksMap::Get())
    return;

  WebFileSystemCallbacks* callbacks =
      CallbacksMap::Get()->GetAndUnregisterCallbacks(callbacks_id);
  DCHECK(callbacks);

  if (file_info.is_directory || file_info.size < 0) {
    callbacks->didFail(WebKit::WebFileErrorInvalidState);
    return;
  }
  callbacks->didCreateFileWriter(
      new WebFileWriterImpl(path, client, main_thread_loop), file_info.size);
}

void CreateSnapshotFileCallbackAdapter(
    int thread_id, int callbacks_id,
    base::MessageLoopProxy* main_thread_loop,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    int request_id) {
  if (thread_id != CurrentWorkerId()) {
    WorkerTaskRunner::Instance()->PostTask(
        thread_id,
        base::Bind(&CreateSnapshotFileCallbackAdapter,
                    thread_id, callbacks_id,
                    make_scoped_refptr(main_thread_loop),
                    file_info, platform_path, request_id));
    return;
  }

  if (!CallbacksMap::Get())
    return;

  WebFileSystemCallbacks* callbacks =
      CallbacksMap::Get()->GetAndUnregisterCallbacks(callbacks_id);
  DCHECK(callbacks);

  WebFileInfo web_file_info;
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = platform_path.AsUTF16Unsafe();
  callbacks->didCreateSnapshotFile(web_file_info);

  // TODO(michaeln,kinuko): Use ThreadSafeSender when Blob becomes
  // non-bridge model.
  main_thread_loop->PostTask(
      FROM_HERE, base::Bind(&DidReceiveSnapshotFile, request_id));
}

}  // namespace

WebFileSystemImpl::~WebFileSystemImpl() {
}

WebFileSystemImpl::WebFileSystemImpl(base::MessageLoopProxy* main_thread_loop)
    : main_thread_loop_(main_thread_loop) {
}

void WebFileSystemImpl::move(
    const WebKit::WebURL& src_path,
    const WebKit::WebURL& dest_path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Move,
      MakeTuple(GURL(src_path), GURL(dest_path),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::copy(
    const WebKit::WebURL& src_path,
    const WebKit::WebURL& dest_path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Copy,
      MakeTuple(GURL(src_path), GURL(dest_path),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::remove(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Remove,
      MakeTuple(GURL(path), false /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::removeRecursively(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Remove,
      MakeTuple(GURL(path), true /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::readMetadata(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadMetadata,
      MakeTuple(GURL(path),
                base::Bind(&ReadMetadataCallbackAdapter,
                           CurrentWorkerId(), callbacks_id),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::createFile(
    const WebKit::WebURL& path,
    bool exclusive,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateFile,
      MakeTuple(GURL(path), exclusive,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::createDirectory(
    const WebKit::WebURL& path,
    bool exclusive,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateDirectory,
      MakeTuple(GURL(path), exclusive, false /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::fileExists(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Exists,
      MakeTuple(GURL(path), false /* directory */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::directoryExists(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Exists,
      MakeTuple(GURL(path), true /* directory */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::readDirectory(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadDirectory,
      MakeTuple(GURL(path),
                base::Bind(&ReadDirectoryCallbackAdapater,
                           CurrentWorkerId(), callbacks_id),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

WebKit::WebFileWriter* WebFileSystemImpl::createFileWriter(
    const WebURL& path, WebKit::WebFileWriterClient* client) {
  return new WebFileWriterImpl(GURL(path), client, main_thread_loop_.get());
}

void WebFileSystemImpl::createFileWriter(
    const WebURL& path,
    WebKit::WebFileWriterClient* client,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadMetadata,
      MakeTuple(GURL(path),
                base::Bind(&CreateFileWriterCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, main_thread_loop_,
                           GURL(path), client),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

void WebFileSystemImpl::createSnapshotFileAndReadMetadata(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateSnapshotFile,
      MakeTuple(GURL(path),
                base::Bind(&CreateSnapshotFileCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           main_thread_loop_),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id)));
}

}  // namespace content
