// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_browser_thread.h"

namespace content {

TestBrowserThread::TestBrowserThread(ID identifier)
    : BrowserThreadImpl(identifier) {
}

TestBrowserThread::TestBrowserThread(ID identifier, MessageLoop* message_loop)
    : BrowserThreadImpl(identifier, message_loop) {
}

TestBrowserThread::~TestBrowserThread() {
}

}  // namespace content
