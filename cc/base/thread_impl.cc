// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/thread_impl.h"

#include "base/message_loop.h"

namespace cc {

scoped_ptr<Thread> ThreadImpl::CreateForCurrentThread() {
  return scoped_ptr<Thread>(
      new ThreadImpl(base::MessageLoopProxy::current())).Pass();
}

scoped_ptr<Thread> ThreadImpl::CreateForDifferentThread(
    scoped_refptr<base::MessageLoopProxy> thread) {
  return scoped_ptr<Thread>(new ThreadImpl(thread)).Pass();
}

ThreadImpl::~ThreadImpl() {}

void ThreadImpl::PostTask(base::Closure cb) {
  thread_->PostTask(FROM_HERE, cb);
}

void ThreadImpl::PostDelayedTask(base::Closure cb, base::TimeDelta delay) {
  thread_->PostDelayedTask(FROM_HERE, cb, delay);
}

bool ThreadImpl::BelongsToCurrentThread() const {
  return thread_->BelongsToCurrentThread();
}

ThreadImpl::ThreadImpl(scoped_refptr<base::MessageLoopProxy> thread)
    : thread_(thread) {}

}  // namespace cc
