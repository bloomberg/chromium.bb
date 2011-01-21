// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_thread.h"

class BrowserWebKitClientImpl;

// This creates a WebKit main thread on instantiation (if not in
// --single-process mode) on construction and kills it on deletion.
class WebKitThread {
 public:
  // Called from the UI thread.
  WebKitThread();
  ~WebKitThread();
  void Initialize();

 private:
  // Must be private so that we can carefully control its lifetime.
  class InternalWebKitThread : public BrowserThread {
   public:
    InternalWebKitThread();
    virtual ~InternalWebKitThread();
    // Does the actual initialization and shutdown of WebKit.  Called at the
    // beginning and end of the thread's lifetime.
    virtual void Init();
    virtual void CleanUp();

   private:
    // The WebKitClient implementation.  Only access on WebKit thread.
    scoped_ptr<BrowserWebKitClientImpl> webkit_client_;
  };

  // Pointer to the actual WebKitThread.
  scoped_ptr<InternalWebKitThread> webkit_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebKitThread);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
