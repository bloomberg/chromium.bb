// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_webkit_thread.h"

#include "base/logging.h"
#include "content/ppapi_plugin/ppapi_webkitplatformsupport_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"

PpapiWebKitThread::PpapiWebKitThread() {
  DCHECK(!webkit_thread_.get());

  webkit_thread_.reset(new InternalWebKitThread);
  bool started = webkit_thread_->Start();
  DCHECK(started);
}

PpapiWebKitThread::~PpapiWebKitThread() {
}

void PpapiWebKitThread::PostTask(const tracked_objects::Location& from_here,
                                 const base::Closure& task) {
  webkit_thread_->message_loop()->PostTask(from_here, task);
}

PpapiWebKitThread::InternalWebKitThread::InternalWebKitThread()
    : base::Thread("PPAPIWebKit") {
}

PpapiWebKitThread::InternalWebKitThread::~InternalWebKitThread() {
  Stop();
}

void PpapiWebKitThread::InternalWebKitThread::Init() {
  DCHECK(!webkit_platform_support_.get());
  webkit_platform_support_.reset(new PpapiWebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());
}

void PpapiWebKitThread::InternalWebKitThread::CleanUp() {
  DCHECK(webkit_platform_support_.get());
  WebKit::shutdown();
}
