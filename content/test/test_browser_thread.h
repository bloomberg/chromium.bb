// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BROWSER_BROWSER_THREAD_H_
#define CONTENT_TEST_TEST_BROWSER_BROWSER_THREAD_H_
#pragma once

#include "content/browser/browser_thread_impl.h"

namespace content {

// A BrowserThread for unit tests; this lets unit tests in chrome/
// create BrowserThread instances.
class TestBrowserThread : public BrowserThreadImpl {
 public:
  explicit TestBrowserThread(ID identifier);
  TestBrowserThread(ID identifier, MessageLoop* message_loop);
  virtual ~TestBrowserThread();
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BROWSER_BROWSER_THREAD_H_
