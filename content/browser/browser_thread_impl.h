// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#define CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#pragma once

#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class CONTENT_EXPORT BrowserThreadImpl
    : public BrowserThread, public base::Thread {
 public:
  // Construct a BrowserThreadImpl with the supplied identifier.  It is an error
  // to construct a BrowserThreadImpl that already exists.
  explicit BrowserThreadImpl(BrowserThread::ID identifier);

  // Special constructor for the main (UI) thread and unittests. We use a dummy
  // thread here since the main thread already exists.
  BrowserThreadImpl(BrowserThread::ID identifier, MessageLoop* message_loop);
  virtual ~BrowserThreadImpl();

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  // We implement all the functionality of the public BrowserThread
  // functions, but state is stored in the BrowserThreadImpl to keep
  // the API cleaner. Therefore make BrowserThread a friend class.
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

  // Common initialization code for the constructors.
  void Initialize();

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
