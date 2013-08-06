// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilesystem_impl.h"

#include "base/bind.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
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

class WaitableCallbackResults {
 public:
  static WaitableCallbackResults* MaybeCreate(
      WebKit::WebFileSystemCallbacks* callbacks) {
    if (callbacks->shouldBlockUntilCompletion())
      return new WaitableCallbackResults;
    return NULL;
  }
  ~WaitableCallbackResults() {}

  void SetResultsAndSignal(const base::Closure& results_closure) {
    results_closure_ = results_closure;
    event_->Signal();
  }

  void WaitAndRun() {
    event_->Wait();
    DCHECK(!results_closure_.is_null());
    results_closure_.Run();
  }

 private:
  WaitableCallbackResults() : event_(new base::WaitableEvent(true, false)) {}

  base::WaitableEvent* event_;
  base::Closure results_closure_;
  DISALLOW_COPY_AND_ASSIGN(WaitableCallbackResults);
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
    Method method, const Params& params,
    scoped_ptr<WaitableCallbackResults> waitable_results) {
  scoped_ptr<WaitableCallbackResults> null_waitable;
  if (!loop->RunsTasksOnCurrentThread()) {
    loop->PostTask(FROM_HERE,
                   base::Bind(&CallDispatcherOnMainThread<Method, Params>,
                              make_scoped_refptr(loop), method, params,
                              base::Passed(&null_waitable)));
    if (!waitable_results)
      return;
    waitable_results->WaitAndRun();
  }
  if (!ChildThread::current() ||
      !ChildThread::current()->file_system_dispatcher())
    return;

  DCHECK(!waitable_results);
  DispatchToMethod(ChildThread::current()->file_system_dispatcher(),
                   method, params);
}

// Run WebFileSystemCallbacks's |method| with |params|.
template <typename Method, typename Params>
void RunCallbacks(int callbacks_id, Method method, const Params& params) {
  if (!CallbacksMap::Get())
    return;
  WebFileSystemCallbacks* callbacks =
      CallbacksMap::Get()->GetAndUnregisterCallbacks(callbacks_id);
  DCHECK(callbacks);
  DispatchToMethod(callbacks, method, params);
}

void DispatchResultsClosure(int thread_id, int callbacks_id,
                            WaitableCallbackResults* waitable_results,
                            const base::Closure& results_closure) {
  if (thread_id != CurrentWorkerId()) {
    if (waitable_results) {
      waitable_results->SetResultsAndSignal(results_closure);
      return;
    }
    WorkerTaskRunner::Instance()->PostTask(thread_id, results_closure);
    return;
  }
  results_closure.Run();
}

template <typename Method, typename Params>
void CallbackFileSystemCallbacks(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    Method method, const Params& params) {
  DispatchResultsClosure(
      thread_id, callbacks_id, waitable_results,
      base::Bind(&RunCallbacks<Method, Params>, callbacks_id, method, params));
}

void StatusCallbackAdapter(int thread_id, int callbacks_id,
                           WaitableCallbackResults* waitable_results,
                           base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK) {
    CallbackFileSystemCallbacks(
        thread_id, callbacks_id, waitable_results,
        &WebFileSystemCallbacks::didSucceed, MakeTuple());
  } else {
    CallbackFileSystemCallbacks(
        thread_id, callbacks_id, waitable_results,
        &WebFileSystemCallbacks::didFail,
        MakeTuple(fileapi::PlatformFileErrorToWebFileError(error)));
  }
}

void ReadMetadataCallbackAdapter(int thread_id, int callbacks_id,
                                 WaitableCallbackResults* waitable_results,
                                 const base::PlatformFileInfo& file_info) {
  WebFileInfo web_file_info;
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id, waitable_results,
      &WebFileSystemCallbacks::didReadMetadata,
      MakeTuple(web_file_info));
}

void ReadDirectoryCallbackAdapater(
    int thread_id, int callbacks_id, WaitableCallbackResults* waitable_results,
    const std::vector<fileapi::DirectoryEntry>& entries,
    bool has_more) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    file_system_entries[i].name =
        base::FilePath(entries[i].name).AsUTF16Unsafe();
    file_system_entries[i].isDirectory = entries[i].is_directory;
  }
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id, waitable_results,
      &WebFileSystemCallbacks::didReadDirectory,
      MakeTuple(file_system_entries, has_more));
}

void DidCreateFileWriter(
    int callbacks_id,
    const GURL& path,
    WebKit::WebFileWriterClient* client,
    base::MessageLoopProxy* main_thread_loop,
    const base::PlatformFileInfo& file_info) {
  if (!CallbacksMap::Get())
    return;

  WebFileSystemCallbacks* callbacks =
      CallbacksMap::Get()->GetAndUnregisterCallbacks(callbacks_id);
  DCHECK(callbacks);

  if (file_info.is_directory || file_info.size < 0) {
    callbacks->didFail(WebKit::WebFileErrorInvalidState);
    return;
  }
  WebFileWriterImpl::Type type = callbacks->shouldBlockUntilCompletion() ?
      WebFileWriterImpl::TYPE_SYNC : WebFileWriterImpl::TYPE_ASYNC;
  callbacks->didCreateFileWriter(
      new WebFileWriterImpl(path, client, type, main_thread_loop),
      file_info.size);
}

void CreateFileWriterCallbackAdapter(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    base::MessageLoopProxy* main_thread_loop,
    const GURL& path,
    WebKit::WebFileWriterClient* client,
    const base::PlatformFileInfo& file_info) {
  DispatchResultsClosure(
      thread_id, callbacks_id, waitable_results,
      base::Bind(&DidCreateFileWriter, callbacks_id, path, client,
                 make_scoped_refptr(main_thread_loop), file_info));
}

void DidCreateSnapshotFile(
    int callbacks_id,
    base::MessageLoopProxy* main_thread_loop,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    int request_id) {
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

void CreateSnapshotFileCallbackAdapter(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    base::MessageLoopProxy* main_thread_loop,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    int request_id) {
  DispatchResultsClosure(
      thread_id, callbacks_id, waitable_results,
      base::Bind(&DidCreateSnapshotFile, callbacks_id,
                 make_scoped_refptr(main_thread_loop),
                 file_info, platform_path, request_id));
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
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Move,
      MakeTuple(GURL(src_path), GURL(dest_path),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::copy(
    const WebKit::WebURL& src_path,
    const WebKit::WebURL& dest_path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Copy,
      MakeTuple(GURL(src_path), GURL(dest_path),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::remove(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Remove,
      MakeTuple(GURL(path), false /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::removeRecursively(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Remove,
      MakeTuple(GURL(path), true /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::readMetadata(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadMetadata,
      MakeTuple(GURL(path),
                base::Bind(&ReadMetadataCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results)),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::createFile(
    const WebKit::WebURL& path,
    bool exclusive,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateFile,
      MakeTuple(GURL(path), exclusive,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::createDirectory(
    const WebKit::WebURL& path,
    bool exclusive,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateDirectory,
      MakeTuple(GURL(path), exclusive, false /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::fileExists(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Exists,
      MakeTuple(GURL(path), false /* directory */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::directoryExists(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Exists,
      MakeTuple(GURL(path), true /* directory */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::readDirectory(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadDirectory,
      MakeTuple(GURL(path),
                base::Bind(&ReadDirectoryCallbackAdapater,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results)),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

WebKit::WebFileWriter* WebFileSystemImpl::createFileWriter(
    const WebURL& path, WebKit::WebFileWriterClient* client) {
  return new WebFileWriterImpl(GURL(path), client,
                               WebFileWriterImpl::TYPE_ASYNC,
                               main_thread_loop_.get());
}

void WebFileSystemImpl::createFileWriter(
    const WebURL& path,
    WebKit::WebFileWriterClient* client,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadMetadata,
      MakeTuple(GURL(path),
                base::Bind(&CreateFileWriterCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results),
                           main_thread_loop_, GURL(path), client),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

void WebFileSystemImpl::createSnapshotFileAndReadMetadata(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  int callbacks_id = CallbacksMap::GetOrCreate()->RegisterCallbacks(callbacks);
  WaitableCallbackResults* waitable_results =
      WaitableCallbackResults::MaybeCreate(callbacks);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateSnapshotFile,
      MakeTuple(GURL(path),
                base::Bind(&CreateSnapshotFileCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results),
                           main_thread_loop_),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id,
                           base::Unretained(waitable_results))),
      make_scoped_ptr(waitable_results));
}

}  // namespace content
