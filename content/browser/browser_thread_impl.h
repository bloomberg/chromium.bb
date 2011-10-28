// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#define CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#pragma once

#include "content/public/browser/browser_thread.h"

namespace content {

class BrowserThreadImpl : public BrowserThread {
 public:
  explicit BrowserThreadImpl(BrowserThread::ID identifier);
  BrowserThreadImpl(BrowserThread::ID identifier, MessageLoop* message_loop);
  virtual ~BrowserThreadImpl();

 private:
  // We implement most functionality on the public set of
  // BrowserThread functions, but state is stored in the
  // BrowserThreadImpl to keep the public API cleaner. Therefore make
  // BrowserThread a friend class.
  friend class BrowserThread;

  // TODO(brettw) remove this variant when Task->Closure migration is complete.
  static bool PostTaskHelper(
      BrowserThread::ID identifier,
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms,
      bool nestable);
  static bool PostTaskHelper(
      BrowserThread::ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms,
      bool nestable);

  // This lock protects |browser_threads_|.  Do not read or modify that array
  // without holding this lock.  Do not block while holding this lock.
  static base::Lock lock_;

  // An array of the BrowserThread objects.  This array is protected by |lock_|.
  // The threads are not owned by this array.  Typically, the threads are owned
  // on the UI thread by the g_browser_process object.  BrowserThreads remove
  // themselves from this array upon destruction.
  static BrowserThread* browser_threads_[ID_COUNT];
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
