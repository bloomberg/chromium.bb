// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/webkit_thread.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/in_process_webkit/browser_webkitclient_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "webkit/glue/webkit_glue.h"

WebKitThread::WebKitThread() {
}

// This happens on the UI thread after the IO thread has been shut down.
WebKitThread::~WebKitThread() {
  // We can't just check CurrentlyOn(BrowserThread::UI) because in unit tests,
  // MessageLoop::Current is sometimes NULL and other times valid and there's
  // no BrowserThread object.  Can't check that CurrentlyOn is not IO since
  // some unit tests set that BrowserThread for other checks.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
}

void WebKitThread::Initialize() {
  DCHECK(!webkit_thread_.get());

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess)) {
    // TODO(jorlow): We need a better story for single process mode.
    return;
  }

  webkit_thread_.reset(new InternalWebKitThread);
  bool started = webkit_thread_->Start();
  DCHECK(started);
}

WebKitThread::InternalWebKitThread::InternalWebKitThread()
    : BrowserThread(BrowserThread::WEBKIT) {
}

WebKitThread::InternalWebKitThread::~InternalWebKitThread() {
  Stop();
}

void WebKitThread::InternalWebKitThread::Init() {
  DCHECK(!webkit_client_.get());
  webkit_client_.reset(new BrowserWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());
  webkit_glue::EnableWebCoreLogChannels(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebCoreLogChannels));

  // If possible, post initialization tasks to this thread (rather than doing
  // them now) so we don't block the UI thread any longer than we have to.
}

void WebKitThread::InternalWebKitThread::CleanUp() {
  DCHECK(webkit_client_.get());
  WebKit::shutdown();
}
