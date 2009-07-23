// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_thread.h"

#include "base/command_line.h"
#include "chrome/browser/in_process_webkit/browser_webkitclient_impl.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/api/public/WebKit.h"

// This happens on the UI thread before the IO thread has been shut down.
WebKitThread::WebKitThread()
    : io_message_loop_(ChromeThread::GetMessageLoop(ChromeThread::IO)) {
  // The thread is started lazily by InitializeThread() on the IO thread.
}

// This happens on the UI thread after the IO thread has been shut down.
WebKitThread::~WebKitThread() {
  // There's no good way to see if we're on the UI thread.
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!io_message_loop_);
}

void WebKitThread::Shutdown() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(io_message_loop_);

  // TODO(jorlow): Start flushing LocalStorage?

  AutoLock lock(io_message_loop_lock_);
  io_message_loop_ = NULL;
}

bool WebKitThread::PostIOThreadTask(
    const tracked_objects::Location& from_here, Task* task) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  {
    AutoLock lock(io_message_loop_lock_);
    if (io_message_loop_) {
      io_message_loop_->PostTask(from_here, task);
      return true;
    }
  }
  delete task;
  return false;
}

WebKitThread::InternalWebKitThread::InternalWebKitThread()
    : ChromeThread(ChromeThread::WEBKIT),
      webkit_client_(NULL) {
}

void WebKitThread::InternalWebKitThread::Init() {
  DCHECK(!webkit_client_);
  webkit_client_ = new BrowserWebKitClientImpl;
  DCHECK(webkit_client_);
  WebKit::initialize(webkit_client_);
  // If possible, post initialization tasks to this thread (rather than doing
  // them now) so we don't block the IO thread any longer than we have to.
}

void WebKitThread::InternalWebKitThread::CleanUp() {
  // TODO(jorlow): Block on LocalStorage being 100% shut down.
  DCHECK(webkit_client_);
  WebKit::shutdown();
  delete webkit_client_;
}

MessageLoop* WebKitThread::InitializeThread() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return NULL;

  DCHECK(io_message_loop_);
  DCHECK(!webkit_thread_.get());
  webkit_thread_.reset(new InternalWebKitThread);
  bool started = webkit_thread_->Start();
  DCHECK(started);
  return webkit_thread_->message_loop();
}
