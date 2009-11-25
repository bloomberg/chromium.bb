// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_thread.h"

#include "base/command_line.h"
#include "chrome/browser/in_process_webkit/browser_webkitclient_impl.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"

// This happens on the UI thread before the IO thread has been shut down.
WebKitThread::WebKitThread() {
  // The thread is started lazily by InitializeThread() on the IO thread.
}

// This happens on the UI thread after the IO thread has been shut down.
WebKitThread::~WebKitThread() {
  // We can't just check CurrentlyOn(ChromeThread::UI) because in unit tests,
  // MessageLoop::Current is sometimes NULL and other times valid and there's
  // no ChromeThread object.  Can't check that CurrentlyOn is not IO since
  // some unit tests set that ChromeThread for other checks.
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
}

void WebKitThread::EnsureInitialized() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (webkit_thread_.get())
    return;
  InitializeThread();
}

WebKitThread::InternalWebKitThread::InternalWebKitThread()
    : ChromeThread(ChromeThread::WEBKIT) {
}

WebKitThread::InternalWebKitThread::~InternalWebKitThread() {
  Stop();
}

void WebKitThread::InternalWebKitThread::Init() {
  DCHECK(!webkit_client_.get());
  webkit_client_.reset(new BrowserWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());
  // If possible, post initialization tasks to this thread (rather than doing
  // them now) so we don't block the IO thread any longer than we have to.
}

void WebKitThread::InternalWebKitThread::CleanUp() {
  DCHECK(webkit_client_.get());
  WebKit::shutdown();
}

MessageLoop* WebKitThread::InitializeThread() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return NULL;

  DCHECK(!webkit_thread_.get());
  webkit_thread_.reset(new InternalWebKitThread);
  bool started = webkit_thread_->Start();
  DCHECK(started);
  return webkit_thread_->message_loop();
}
