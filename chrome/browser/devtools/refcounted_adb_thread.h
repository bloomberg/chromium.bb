// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REFCOUNTED_ADB_THREAD_H_
#define CHROME_BROWSER_DEVTOOLS_REFCOUNTED_ADB_THREAD_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"

class RefCountedAdbThread : public base::RefCounted<RefCountedAdbThread> {
 public:
  static scoped_refptr<RefCountedAdbThread> GetInstance();
  base::MessageLoop* message_loop();

 private:
  friend class base::RefCounted<RefCountedAdbThread>;
  static RefCountedAdbThread* instance_;
  static void StopThread(base::Thread* thread);

  RefCountedAdbThread();
  virtual ~RefCountedAdbThread();
  base::Thread* thread_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_REFCOUNTED_ADB_THREAD_H_
