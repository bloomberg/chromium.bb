// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#define CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_

#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class CONTENT_EXPORT BrowserThreadImpl : public BrowserThread,
                                         public base::Thread {
 public:
  // Construct a BrowserThreadImpl with the supplied identifier.  It is an error
  // to construct a BrowserThreadImpl that already exists.
  explicit BrowserThreadImpl(BrowserThread::ID identifier);

  // Special constructor for the main (UI) thread and unittests. We use a dummy
  // thread here since the main thread already exists.
  BrowserThreadImpl(BrowserThread::ID identifier, MessageLoop* message_loop);
  virtual ~BrowserThreadImpl();

  static void ShutdownThreadPool();

 protected:
  virtual void Init() OVERRIDE;
  virtual void Run(MessageLoop* message_loop) OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  // We implement all the functionality of the public BrowserThread
  // functions, but state is stored in the BrowserThreadImpl to keep
  // the API cleaner. Therefore make BrowserThread a friend class.
  friend class BrowserThread;

  // The following are unique function names that makes it possible to tell
  // the thread id from the callstack alone in crash dumps.
  void UIThreadRun(MessageLoop* message_loop);
  void DBThreadRun(MessageLoop* message_loop);
  void WebKitThreadRun(MessageLoop* message_loop);
  void FileThreadRun(MessageLoop* message_loop);
  void FileUserBlockingThreadRun(MessageLoop* message_loop);
  void ProcessLauncherThreadRun(MessageLoop* message_loop);
  void CacheThreadRun(MessageLoop* message_loop);
  void IOThreadRun(MessageLoop* message_loop);

  static bool PostTaskHelper(
      BrowserThread::ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay,
      bool nestable);

  // Common initialization code for the constructors.
  void Initialize();

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
