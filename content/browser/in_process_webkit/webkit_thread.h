// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/common/content_export.h"

class BrowserWebKitPlatformSupportImpl;

// This creates a WebKit main thread on instantiation (if not in
// --single-process mode) on construction and kills it on deletion.
class CONTENT_EXPORT WebKitThread {
 public:
  // Called from the UI thread.
  WebKitThread();
  ~WebKitThread();
  void Initialize();

 private:
  // Must be private so that we can carefully control its lifetime.
  class InternalWebKitThread : public content::BrowserThreadImpl {
   public:
    InternalWebKitThread();
    virtual ~InternalWebKitThread();
    // Does the actual initialization and shutdown of WebKit.  Called at the
    // beginning and end of the thread's lifetime.
    virtual void Init() OVERRIDE;
    virtual void CleanUp() OVERRIDE;

   private:
    // The WebKitPlatformSupport implementation.  Only access on WebKit thread.
    scoped_ptr<BrowserWebKitPlatformSupportImpl> webkit_platform_support_;
  };

  // Pointer to the actual WebKitThread.
  scoped_ptr<InternalWebKitThread> webkit_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebKitThread);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
