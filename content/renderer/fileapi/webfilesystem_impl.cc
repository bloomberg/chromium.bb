// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fileapi/webfilesystem_impl.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/fileapi/file_system_dispatcher.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/blink/public/web/web_frame.h"

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<WebFileSystemImpl>>::Leaky
    g_webfilesystem_tls = LAZY_INSTANCE_INITIALIZER;

enum CallbacksUnregisterMode {
  UNREGISTER_CALLBACKS,
  DO_NOT_UNREGISTER_CALLBACKS,
};

}  // namespace

//-----------------------------------------------------------------------------
// WebFileSystemImpl

WebFileSystemImpl* WebFileSystemImpl::ThreadSpecificInstance(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  if (g_webfilesystem_tls.Pointer()->Get() || !main_thread_task_runner)
    return g_webfilesystem_tls.Pointer()->Get();
  WebFileSystemImpl* filesystem =
      new WebFileSystemImpl(std::move(main_thread_task_runner));
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(filesystem);
  return filesystem;
}

void WebFileSystemImpl::DeleteThreadSpecificInstance() {
  DCHECK(!WorkerThread::GetCurrentId());
  if (g_webfilesystem_tls.Pointer()->Get())
    delete g_webfilesystem_tls.Pointer()->Get();
}

WebFileSystemImpl::WebFileSystemImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(main_thread_task_runner),
      file_system_dispatcher_(std::move(main_thread_task_runner)) {
  g_webfilesystem_tls.Pointer()->Set(this);
}

WebFileSystemImpl::~WebFileSystemImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_webfilesystem_tls.Pointer()->Set(nullptr);
}

void WebFileSystemImpl::WillStopCurrentWorkerThread() {
  delete this;
}

void WebFileSystemImpl::ChooseEntry(
    blink::WebFrame* frame,
    std::unique_ptr<ChooseEntryCallbacks> callbacks) {
  file_system_dispatcher_.ChooseEntry(
      RenderFrame::GetRoutingIdForWebFrame(frame), std::move(callbacks));
}

}  // namespace content
