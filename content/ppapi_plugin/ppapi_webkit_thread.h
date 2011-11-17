// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_WEBKIT_THREAD_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_WEBKIT_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"

class PpapiWebKitPlatformSupportImpl;

// This creates a WebKit main thread on instantiation on construction and kills
// it on deletion. See also content/browser/in_process_webkit for the
// corresponding concept in the browser process.
class PpapiWebKitThread {
 public:
  PpapiWebKitThread();
  ~PpapiWebKitThread();

  // Posts the given task to the thread, see MessageLoop::PostTask.
  void PostTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

 private:
  // Must be private so that we can carefully control its lifetime.
  class InternalWebKitThread : public base::Thread {
   public:
    InternalWebKitThread();
    virtual ~InternalWebKitThread();
    // Does the actual initialization and shutdown of WebKit.  Called at the
    // beginning and end of the thread's lifetime.
    virtual void Init() OVERRIDE;
    virtual void CleanUp() OVERRIDE;

   private:
    // The WebKitPlatformSupport implementation.  Only access on WebKit thread.
    scoped_ptr<PpapiWebKitPlatformSupportImpl> webkit_platform_support_;
  };

  // Pointer to the actual WebKitThread.
  scoped_ptr<InternalWebKitThread> webkit_thread_;

  DISALLOW_COPY_AND_ASSIGN(PpapiWebKitThread);
};

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_WEBKIT_THREAD_H_
