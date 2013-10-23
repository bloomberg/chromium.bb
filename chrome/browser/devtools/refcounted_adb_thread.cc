// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/refcounted_adb_thread.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";

RefCountedAdbThread* RefCountedAdbThread::instance_ = NULL;

// static
scoped_refptr<RefCountedAdbThread> RefCountedAdbThread::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!instance_)
    new RefCountedAdbThread();
  return instance_;
}

RefCountedAdbThread::RefCountedAdbThread() {
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = NULL;
  }
}

base::MessageLoop* RefCountedAdbThread::message_loop() {
  return thread_ ? thread_->message_loop() : NULL;
}

// static
void RefCountedAdbThread::StopThread(base::Thread* thread) {
  thread->Stop();
}

RefCountedAdbThread::~RefCountedAdbThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = NULL;
  if (!thread_)
    return;
  // Shut down thread on FILE thread to join into IO.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RefCountedAdbThread::StopThread, thread_));
}
